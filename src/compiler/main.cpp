#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <array>
#include <sol.hpp>

#include "threadpool.h"
#include "util.h"
#include "templates.h"
#include "logger.h"

struct ModuleDesc {
	std::string name;
	fs::path rootPath;
	std::vector<ScriptDesc> scripts;
	bool compile;
};

struct CompilerState {
	std::vector<ModuleDesc> modules;
	ThreadPool pool;
};

void compileScript(const std::string& path, std::vector<u8>& data) {
	sol::state ctx;
	sol::load_result lr = ctx.load_file(path);
	if (lr.valid()) {
		sol::protected_function target = lr;
		sol::bytecode byteCode = target.dump();
		std::string_view view = byteCode.as_string_view();

		data.resize(view.size());
		memcpy(data.data(), view.data(), view.size());
	} else {
		assert(false);
	}
}

void loadScript(const std::string& path, std::vector<u8>& target) {
	std::string data = readTextFile(path);
	target.resize(data.size());
	memcpy(target.data(), data.data(), data.size());
}

void processModule(ModuleDesc& module, CompilerState& state) {
	for (size_t i = 0; i < module.scripts.size(); ++i) {
		ScriptDesc* s = &module.scripts[i];
		std::string fullPath = (module.rootPath / s->path).make_preferred().string();
		//Logger::log(module.name + "::" + s->name);
		if (module.compile) {
			compileScript(fullPath, s->data);
		} else {
			loadScript(fullPath, s->data);
		}
	}
}

#include "xxhash.h"

void writeHeaderFile(CompilerState& state, const fs::path& targetDir) {
	fs::path targetHeaderPath = targetDir / "CompiledScripts.h";

	std::stringstream ss;
	ss << HEADER_CODE_TEMPLATE;

	for (auto& module : state.modules) {
		ss << "namespace " << module.name << " {";
		ss << HEADER_FUNCS_TEMPLATE << "}" << std::endl << std::endl;
	}

	ss << "}" << std::endl;

	std::string code = ss.str();
	std::string fileData = readTextFile(targetHeaderPath);

	XXH64_hash_t codeHash = XXH64(code.data(), code.size(), 1337);
	XXH64_hash_t fileHash = XXH64(fileData.data(), fileData.size(), 1337);

	if (codeHash != fileHash) {
		Logger::log("Writing " + targetHeaderPath.string());
		std::ofstream outf(targetHeaderPath);
		if (!outf.is_open()) {
			Logger::log("Failed to open file");
			return;
		}

		outf << code;
	}
}

void writeSourceFile(const ModuleDesc& module, const fs::path& targetDir) {
	fs::path targetFile = targetDir / "CompiledScripts_";
	targetFile += module.name + ".cpp";

	std::stringstream vars;
	std::stringstream lookup;

	lookup << "ScriptLookup _lookup = {" << std::endl;

	for (const ScriptDesc& compiled : module.scripts) {
		if (compiled.data.size() > 0) {
			vars << "const std::uint8_t " << compiled.varName << "[] = { ";
			for (size_t i = 0; i < compiled.data.size(); ++i) {
				if (i != 0) vars << ", ";
				vars << (u32)compiled.data[i];
			}

			vars << " };" << std::endl;

			lookup << "\t{ \"" << 
				compiled.name << "\", { " << 
				compiled.varName << ", " << 
				compiled.data.size() << ", " << 
				(module.compile ? "true" : "false") << " } }," << 
				std::endl;
		}
	}

	lookup << "};" << std::endl;

	std::stringstream ss;
	ss << SOURCE_HEADER_TEMPLATE;
	ss << module.name << " {" << std::endl << std::endl;
	ss << vars.str() << std::endl;
	ss << lookup.str();
	ss << SOURCE_FOOTER_TEMPLATE;

	std::string code = ss.str();
	std::string fileData = readTextFile(targetFile);

	XXH64_hash_t codeHash = XXH64(code.data(), code.size(), 1337);
	XXH64_hash_t fileHash = XXH64(fileData.data(), fileData.size(), 1337);

	if (codeHash != fileHash) {
		Logger::log("Writing " + targetFile.string());
		std::ofstream outf(targetFile);
		outf << code;
	}
}

int main(int argc, char** argv) {
	auto startTime = std::chrono::high_resolution_clock::now();

	{
		CompilerState state;

		fs::path configPath = parsePath(argv[1]);
		fs::path configDir = configPath.parent_path();

		if (!fs::exists(configPath)) {
			std::cout << "No config found at " << configPath << std::endl;
			return 1;
		}

		sol::state ctx;
		sol::protected_function_result res = ctx.do_file(configPath.string());
		if (!res.valid()) {
			sol::error err = res;
			std::string what = "Failed to load config: ";
			throw std::runtime_error(what + err.what());
		}

		sol::table obj = res.get<sol::table>();

		sol::table settings = obj["settings"];
		sol::table modules = obj["modules"];

		fs::path targetDir = (configDir / settings["outDir"].get<std::string>()).make_preferred();
		fs::create_directories(targetDir);

		size_t objSize = 0;
		for (const auto& v : modules) objSize++;

		state.modules.resize(objSize);

		size_t idx = 0;
		for (const auto& v : modules) {
			ModuleDesc& module = state.modules[idx++];

			std::string name = v.first.as<std::string>();
			sol::table settings = v.second.as<sol::table>();

			fs::path path = settings["path"].get<std::string>();
			sol::optional<bool> compileOpt = settings["compile"];

			module.name = name;
			module.compile = !compileOpt.has_value() || compileOpt.value();
			module.rootPath = configDir / path.make_preferred();
			parseDirectory(module.rootPath.string(), module.scripts);
		}

		size_t threadCount = std::min(state.modules.size(), (size_t)std::thread::hardware_concurrency());
		state.pool.start(threadCount);

		for (auto& ns : state.modules) {
			state.pool.enqueue([&]() { 
				processModule(ns, state);
				writeSourceFile(ns, targetDir); 
			});
		}

		writeHeaderFile(state, targetDir);

		state.pool.wait();

		auto endTime = std::chrono::high_resolution_clock::now();
		auto ms = endTime - startTime;
		std::cout << "Lua compile time: " << std::chrono::duration_cast<std::chrono::milliseconds>(ms).count() << "ms" << std::endl;
	}

	return 0;
}
