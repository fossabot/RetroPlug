local componentDescDefaults = {
	global = false
}

local ACTION_IGNORE = { "_", "isA", "init" }

local function startsWith(str, start)
	return str:sub(1, #start) == start
 end

function component(desc)
	if desc.name == nil then
		error("A component descriptor must specify a name")
	end

	for k, v in pairs(componentDescDefaults) do
		if desc[k] == nil then
			desc[k] = v
		end
	end

	local c = {}
	c.__index = c
	c.__desc = desc

	c.registerActions = function(obj, actions)
		for k, action in pairs(getmetatable(actions)) do
			if type(action) == "function" then
				local ignore = false
				for _, v in ipairs(ACTION_IGNORE) do
					if startsWith(k, v) == true then
						ignore = true
						break
					end
				end

				if ignore == false then
					obj.__actions[string.lower(k)] = function(down)
						actions[k](actions, down)
					end
				end
			end
		end
	end

	c.new = function(system, isProjectComponent)
		local obj = { __actions = {}, __enabled = true }
		setmetatable(obj, c)

		if isProjectComponent == true then
			function obj:project() return system end
			function obj:system() return nil end
			function obj:isProject() return true end
		else
			function obj:project() return nil end
			function obj:system() return system end
			function obj:isProject() return false end
		end

		function obj:enabled() return obj.__enabled end
		function obj:setEnabled(enabled) obj.__enabled = enabled end
		if c.init then c.init(obj) end

		return obj
	end

	return c
end
