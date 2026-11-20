broadcastLua([[
	local v = {
		-0.5, -0.5, 0.5,
		0.5, -0.5, 0.5,
		0.5, 0.5, 0.5,
		-0.5, 0.5, 0.5,

		-0.5, -0.5, -0.5,
		0.5, -0.5, -0.5,
		0.5, 0.5, -0.5,
		-0.5, 0.5, -0.5
	}

	local i = {
		0, 1, 2,
		2, 3, 0,
		1, 5, 6,
		6, 2, 1,
		5, 4, 7,
		7, 6, 5,
		4, 0, 3,
		3, 7, 4,
		3, 2, 6,
		6, 7, 3,
		4, 5, 1,
		1, 0, 4
	}
	
	local cube = createObject3D(v, i)
	cube:setColor(0.5, 0.0, 0.0, 1.0)
	cube:setPosition(0.0, 5.0, 0.0)

	table.insert(onRender, function(dt)
		local x, y, z = cube:getPosition()
		cube:setPosition(x + dt*1.5, y, z)
	end)
]], 0)
