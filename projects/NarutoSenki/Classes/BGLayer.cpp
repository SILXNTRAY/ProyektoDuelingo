#include "BGLayer.h"

void BGLayer::initBg(int mapId, bool isDuel)
{
	if (isDuel)
	{
		// Duel maps now have their own dedicated backgrounds living
		// alongside them in Maps/Duels/ (map_bg1.png, map_bg2.png, ...),
		// one per duel map — no more falling back to the regular pool's
		// map_bg1.png like before this folder split.
		bgMap = Sprite::create(format("Maps/Duels/map_bg{}.png", mapId).c_str());
	}
	else
	{
		bgMap = Sprite::create(GetMapBgPath(mapId));
	}
	bgMap->setAnchorPoint(Vec2(0, 0));
	bgMap->setPosition(Vec2(0, 192));
	addChild(bgMap);
}