#include "UiLuaContext.h"

#include <chrono>

#include "plugs/SameBoyPlug.h"

#include "util/fs.h"
#include "util/DataBuffer.h"
#include "util/File.h"
#include "model/FileManager.h"
#include "util/base64enc.h"
#include "util/base64dec.h"
#include "config.h"
#include "platform/Menu.h"

#include "platform/Platform.h"
#include "LuaHelpers.h"
#include "platform/Logger.h"
#include "Wrappers.h"

#ifdef COMPILE_LUA_SCRIPTS
#include "generated/CompiledScripts.h"
#endif

void UiLuaContext::init(AudioContextProxy* proxy, const std::string& path, const std::string& scriptPath) {
	_configPath = path;
	_scriptPath = scriptPath;
	_proxy = proxy;
	setup(true);
}

void UiLuaContext::update(double delta) {
	if (_reload) {
		reload();
		_reload = false;
	}

	if (!_haltFrameProcessing) {
		//_haltFrameProcessing = !callFunc(_state, "_frame", delta);
	}
}

bool UiLuaContext::onKey(VirtualKey key, bool down) {
	bool res = false;
	callFuncRet(_viewRoot, "onKey", res, key, down);
	return res;
}

void UiLuaContext::onDoubleClick(float x, float y, MouseMod mod) {
	callFunc(_viewRoot, "onDoubleClick", x, y, mod);
}

void UiLuaContext::onMouseDown(float x, float y, MouseMod mod) {
	callFunc(_viewRoot, "onMouseDown", x, y, mod);
}

void UiLuaContext::onPadButton(int button, bool down) {
	callFunc(_viewRoot, "onPadButton", button, down);
}

void UiLuaContext::onDrop(float x, float y, const char* str) {
	std::vector<std::string> paths = { str };
	callFunc(_viewRoot, "onDrop", x, y, paths);
}

void UiLuaContext::onMenu(std::vector<Menu*>& menus) {
	callFunc(_viewRoot, "onMenu", menus);
}

void UiLuaContext::onMenuResult(int id) {
	callFunc(_viewRoot, "onMenuResult", id);
}

void UiLuaContext::reload() {
	if (_valid) {
		callFunc(_viewRoot, "onReloadBegin");
	}
	
	shutdown();
	setup(false);

	if (_valid) {
		callFunc(_viewRoot, "onReloadEnd");
	}
	
	_haltFrameProcessing = !_valid;
}

void UiLuaContext::shutdown() {
	if (_state) {
		_viewRoot = sol::table();
		delete _state;
		_state = nullptr;
	}
}

void UiLuaContext::handleDialogCallback(const std::vector<std::string>& paths) {
	callFunc(_viewRoot, "onDialogResult", paths);
}

DataBufferPtr UiLuaContext::saveState() {
	DataBufferPtr buffer = std::make_shared<DataBuffer<char>>();
	callFunc(_viewRoot, "saveState", buffer);
	return buffer;
}

void UiLuaContext::loadState(DataBufferPtr buffer) {
	callFunc(_viewRoot, "loadState", buffer);
}

bool UiLuaContext::setup(bool updateProject) {
	consoleLogLine("------------------------------------------");
	consoleLogLine("Initializing UI lua context");

	_valid = false;
	_state = new sol::state();
	sol::state& s = *_state;

	s.open_libraries(	sol::lib::base, sol::lib::package, sol::lib::table, sol::lib::string, 
						sol::lib::math, sol::lib::debug, sol::lib::coroutine, sol::lib::io	);

	std::string packagePath = s["package"]["path"];
	packagePath += ";" + _configPath + "/?.lua";

#ifdef COMPILE_LUA_SCRIPTS
	consoleLogLine("Using precompiled lua scripts");
	s.add_package_loader(CompiledScripts::common::loader);
	s.add_package_loader(CompiledScripts::ui::loader);
#else
	consoleLogLine("Loading lua scripts from disk");
	packagePath += ";" + _scriptPath + "/common/?.lua";
	packagePath += ";" + _scriptPath + "/ui/?.lua";
#endif

	s["package"]["path"] = packagePath;
	
	luawrappers::registerCommon(s);
	luawrappers::registerChrono(s);
	luawrappers::registerLsdj(s);
	luawrappers::registerZipp(s);
	luawrappers::registerRetroPlug(s);

	s.create_named_table("base64",
		"encode", base64::encode,
		"encodeBuffer", base64::encodeBuffer,
		"decode", base64::decode,
		"decodeBuffer", base64::decodeBuffer
	);

	s.new_usertype<FileManager>("FileManager",
		"loadFile", &FileManager::loadFile,
		"saveFile", &FileManager::saveFile,
		"saveTextFile", &FileManager::saveTextFile,
		"exists", &FileManager::exists
	);

	s.new_usertype<File>("File",
		"data", sol::readonly(&File::data),
		"checksum", sol::readonly(&File::checksum)
	);

	// TODO: Fix naming of this
	s.new_usertype<ViewWrapper>("ViewWrapper",
		"requestDialog", &ViewWrapper::requestDialog,
		"requestMenu", &ViewWrapper::requestMenu
	);

	// TODO: Fix naming of this too!
	s.create_named_table("nativeutil", 
		"mergeMenu", mergeMenu
	);

	s["LUA_MENU_ID_OFFSET"] = LUA_UI_MENU_ID_OFFSET;

	if (!runScript(s, "require('main')")) {
		return false;
	}

	if (!callFuncRet(s, "_getView", _viewRoot)) {
		return false;
	}

	consoleLogLine("Looking for components...");

#ifdef COMPILE_LUA_SCRIPTS
	std::vector<std::string_view> names;
	CompiledScripts::ui::getScriptNames(names);
	loadComponentsFromBinary(s, names);
#else
	loadComponentsFromFile(s, _scriptPath + "/ui/components/");
#endif

	consoleLogLine("Finished loading components");

	// Set up the lua context
	if (!callFunc(_viewRoot, "setup", &_viewWrapper, _proxy)) {
		consoleLogLine("Failed to setup view");
	}

	// Load the users config settings
	std::string configPath = _configPath + "/config.lua";
	if (!callFunc(_viewRoot, "loadConfigFromPath", configPath, updateProject)) {
		consoleLogLine("Failed to load config from " + configPath);
		assert(false);
	}

	loadInputMaps(s, _configPath + "/input");

	_valid = true;
	return true;
}
