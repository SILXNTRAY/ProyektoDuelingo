-- lua/ui/GameModeLayer.lua
function GameModeLayer:init()
    log('Initial GameModeLayer...')
    local width, height = display.width, display.height

    -- Background
    local bg = display.newSprite('red_bg.png', 0, 0)
    bg:setAnchorPoint(0, 0)
    bg:fullScreen()
    self:addChild(bg, -100)

    -- Menu bars
    local bar_b = display.newSprite('menu_bar2.png')
    bar_b:setAnchorPoint(0, 0)
    bar_b:fullScreen()
    self:addChild(bar_b, 2)

    local bar_t = display.newSprite('menu_bar3.png')
    bar_t:setAnchorPoint(0, 0)
    bar_t:setPosition(0, height - bar_t:getContentSize().height)
    bar_t:fullScreen()
    self:addChild(bar_t, 2)

    -- Title sprite
    local title = display.newSprite('#startmenu_title.png')
    title:setAnchorPoint(0, 0)
    title:setPosition(2, height - title:getContentSize().height - 2)
    self:addChild(title, 3)

-- Mode label at bottom
    local modeLabel = CCLabelTTF:create('', 'Arial', 14)
    modeLabel:setAnchorPoint(0, 0)
    modeLabel:setPosition(10, 2)
    self:addChild(modeLabel, 5)

    local modeNames = {
        '1 VS 1',
        '3 VS 3 | Classic Mode',
        '4 VS 4',
        'HardCore (4 VS 4) | Disabled gear',
        'Boss (3 VS 3)',
        'Clone (3 VS 3)',
        'Deathmatch (3 VS 3)',
        'Random Deathmatch (3 VS 3)',
    }

    -- Position constants
    local offset = (width - 460) / 2 + 50
    local posY   = height / 2 + 30
    local locked = { false, false, false, false, true, false, true, false }
    local selectedIdx = nil
    local allItems = {}

    -- Left column: modes 1-3, stacked vertically
    -- Left column: modes 1-3, stacked vertically
    for i = 1, 3 do
        local idx = i - 1
        local item = ui.newImageMenuItem({
            image    = 'GameMode/' .. i .. '.png',
            listener = function()
                if selectedIdx == idx then
                    audio.playSound('Audio/Menu/confirm.ogg')
                    self:selectMode(idx)
                else
                    audio.playSound(ns.menu.SELECT_SOUND)
                    selectedIdx = idx
                    self:selectMode(idx)
                    modeLabel:setString(modeNames[i])
                end
            end
        })
        item:setPosition(0, 0)
        if locked[i] then item:setEnabled(false) end
        local m = ui.newMenu({item})
        m:setPosition(offset, (posY + 55 + 7.5) - (i - 1) * (55 + 7.5))
        self:addChild(m, 1)
    end

    -- Middle row: modes 4-6, spread horizontally
    for i = 4, 6 do
        local idx = i - 1
        local item = ui.newImageMenuItem({
            image    = 'GameMode/' .. i .. '.png',
            listener = function()
                if selectedIdx == idx then
                    audio.playSound('Audio/Menu/confirm.ogg')
                    self:selectMode(idx)
                else
                    audio.playSound(ns.menu.SELECT_SOUND)
                    selectedIdx = idx
                    self:selectMode(idx)
                    modeLabel:setString(modeNames[i])
                end
            end
        })
        item:setPosition(0, 0)
        if locked[i] then item:setEnabled(false) end
        local m = ui.newMenu({item})
        m:setPosition(offset - 75 + (i - 2) * (80 + 5), posY)
        self:addChild(m, 1)
    end

    -- Right column: modes 7-8, stacked vertically
    for i = 7, 8 do
        local idx = i - 1
        local item = ui.newImageMenuItem({
            image    = 'GameMode/' .. i .. '.png',
            listener = function()
                if selectedIdx == idx then
                    audio.playSound('Audio/Menu/confirm.ogg')
                    self:selectMode(idx)
                else
                    audio.playSound(ns.menu.SELECT_SOUND)
                    selectedIdx = idx
                    self:selectMode(idx)
                    modeLabel:setString(modeNames[i])
                end
            end
        })
        item:setPosition(0, 0)
        if locked[i] then item:setEnabled(false) end
        local m = ui.newMenu({item})
        m:setPosition(offset + 20 + (80 + 5) * 4, (posY + 47) - (i - 7) * (86 + 8.0))
        self:addChild(m, 1)
    end

    local modeMenu = ui.newMenu(allItems)
    modeMenu:setPosition(0, 0)
    self:addChild(modeMenu, 1)

    -- Return button
    local ret = ui.newImageMenuItem({
        image    = 'UI/return_btn.png',
        listener = backToStartMenu
    })
    local ret_menu = ui.newMenu({ret})
    ret_menu:setPosition(width - 38, 65)
    self:addChild(ret_menu, 5)
end

function enterGameModeLayer()
    local scene = CCScene:create()
    local layer = GameModeLayer:create()
    hook.registerInitHandlerOnly(layer)
    scene:addChild(layer)
    director.replaceSceneWithFade(scene, 1.25)
end