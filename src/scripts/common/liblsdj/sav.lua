local class = require("class")
local fs = require("fs")
local pathutil = require("pathutil")

local LSDJ_SAV_SIZE = 131072
local LSDJ_SAV_PROJECT_COUNT = 32

local Sav = class()
function Sav:init(savData)
	if savData ~= nil then
		if type(savData) == "string" then
			local data, err = fs.load(savData)
			if err == lsdj_error_t.SUCCESS then
				savData = data
			else
				-- Failed to load file
			end
		end

		local sav, err = liblsdj.sav_read_from_memory(savData)
		if err == lsdj_error_t.SUCCESS then
			self._sav = sav
		else
			-- Failed to read sav
		end
	else
		local sav, err = liblsdj.sav_new()
		if err == lsdj_error_t.SUCCESS then
			self._sav = sav
		else
			-- Failed to create new sav
		end
	end
end

function Sav:isValid()
	return self._sav ~= nil
end

function Sav:loadSong(songIdx)
	local err = liblsdj.sav_set_working_memory_song_from_project(self._sav, songIdx)
	if err ~= lsdj_error_t.SUCCESS then
		return "Failed to load song at index " .. tostring(songIdx)
	end
end

function Sav:toBuffer()
	local buffer = DataBuffer.new()
	local err = liblsdj.sav_write_to_memory(self._sav, buffer)
	if err == lsdj_error_t.SUCCESS then
		return buffer
	end

	return "Failed to write sav to buffer"
end

function Sav:importSongs(items)
	-- Items can contain: a path, a buffer, or an array of paths and buffers
	local t = type(items)
	if t == "string" then
		-- Import song from path.  Path could be a .lsdsng or .sav
		local data = fs.load(items)
		if data ~= nil then
			local ext = pathutil.ext(items)
			if ext == ".lsdsng" then
				self:_importSongFromBuffer(data)
			elseif ext == ".sav" then
				self:_importSongsFromSav(data)
			else
				print("Resource has an unknown file extension " .. ext)
			end
		end
	elseif t == "table" then
		for _, v in ipairs(items) do
			self:importKits(v)
		end
	else--if t == "userdata" then
		if items:size() == LSDJ_SAV_SIZE then
			self:_importSongsFromSav(items)
		else
			self:_importSongFromBuffer(items)
		end
	end
end

function Sav:exportSongs(path)
	for i = 1, LSDJ_SAV_PROJECT_COUNT - 1, 1 do
		local project = liblsdj.sav_get_project(self._sav, i)
		if checkUserData(project) == true then
			local buffer = DataBuffer.new()
			local err = liblsdj.project_write_lsdsng_to_memory(project, buffer)
			if err == lsdj_error_t.SUCCESS then
				local name = liblsdj.project_get_name(project)
				local p = pathutil.join(path, name .. ".lsdsng")
				print("Writing song to file:", p)
				fs.save(p, buffer)
			end
		end
	end
end

function Sav:exportSong(songIdx, path)
	local project = liblsdj.sav_get_project(self._sav, songIdx)
	if checkUserData(project) == true then
		local buffer = DataBuffer.new()
		local err = liblsdj.project_write_lsdsng_to_memory(project, buffer)
		if err == lsdj_error_t.SUCCESS then
			print("Writing song to file:", path)
			fs.save(path, buffer)
		end
	end
end

function Sav:_importSongsFromSav(savData)
	local other = Sav(savData)
	for i = 1, LSDJ_SAV_PROJECT_COUNT - 1, 1 do
		local songData = other:songToBuffer(i)
		self:_importSongFromBuffer(songData)
	end
end

function Sav:_importSongFromBuffer(songData, songIdx)
	songIdx = songIdx or self:nextAvailableProject()
	local proj, err = liblsdj.project_read_lsdsng_from_memory(songData)
	if err == lsdj_error_t.SUCCESS then
		err = liblsdj.sav_set_project(self._sav, proj)
		if err ~= lsdj_error_t.SUCCESS then
			-- Fail
		end
	else
		-- Fail
	end
end

return Sav
