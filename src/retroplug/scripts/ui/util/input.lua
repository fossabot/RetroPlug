local util = require("util")

local module = {}

local function matchCombo(combos, pressed)
	for combo, v in pairs(combos) do
		if #combo == #pressed then
			local match = true
			for i = 1, #combo, 1 do
				if combo[i] ~= pressed[i] then
					match = false
					break
				end
			end

			if match == true then
				return v
			end
		end
	end
end

function module.handleInput(map, key, down, pressed, hooks, buttonStream)
	local handled = false

	-- Do a basic map from key to button
	local found = map.lookup[key]
	if found ~= nil then
		if type(found) == "function" then
			if found(down) ~= false then handled = true end
		elseif buttonStream ~= nil then
			for _, v in ipairs(hooks) do
				if v.fn(v.obj, found, down) ~= false then
					handled = true
				end
			end


			if handled == false then
				if down == true then
					buttonStream:holdDuration(found, 0)
				else
					buttonStream:releaseDuration(found, 0)
				end

				handled = true
			end
		end
	end

	-- If the key is being pressed look for combos
	if down == true then
		table.insert(pressed, key)
		local func = matchCombo(map.combos, pressed)
		if func ~= nil and type(func) == "function" then
			if func(down) ~= false then
				handled = true
			end
		end
	else
		util.tableRemoveElement(pressed, key)
	end

	return handled
end

function module.inputMap(config, map)
	if map == nil then
		map = config
		config = {}
	end

	local lookup = {}
	local combos = {}

	for k, v in pairs(map) do
		if type(k) == "number" then
			lookup[k] = v
		elseif type(k) == "table" then
			combos[k] = v
		end
	end

	return { config = config, lookup = lookup, combos = combos }
end

local function concatTarget(target, map, actions)
	for k, v in pairs(map) do
		if type(v) == "number" then
			target[k] = v
		elseif type(v) == "table" then
			local c = actions[string.lower(v.component)]
			if c ~= nil then
				local f = c[string.lower(v.action)]
				if f ~= nil then
					target[k] = f
				else
					print("Failed to find Action." .. v.component .. "." .. v.action)
				end
			else
				print("Failed to find Action." .. v.component .. "." .. v.action)
			end
		end
	end
end

function module.mergeInputMaps(source, target, actions, romName)
	for _, map in ipairs(source) do
		local merge = false
		if map.config.romName == nil then
			merge = true
		elseif romName:find(map.config.romName) then
			merge = true
		end

		if merge == true then
			concatTarget(target.lookup, map.lookup, actions)
			concatTarget(target.combos, map.combos, actions)
		end
	end
end

return module