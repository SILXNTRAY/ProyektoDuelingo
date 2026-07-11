#include "BGLayer.h"

// AFTER
void BGLayer::initBg(int mapId)
{
	int bgId = (mapId == 6) ? 1 : mapId;
	bgMap = Sprite::create(GetMapBgPath(bgId));
	bgMap->setAnchorPoint(Vec2(0, 0));
	bgMap->setPosition(Vec2(0, 192));
	addChild(bgMap);
}
