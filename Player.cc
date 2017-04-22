#include <SDL2/SDL.h>
#include "Player.hh"
#include "Item.hh"
#include "AllTheItems.hh"
#include <iostream>

using namespace std;

// Constructor
Player::Player() : inventory(10, 6), trash(1, 1) {
    // Set the drag to not 0
    drag.x = 0.3;
    drag.y = 11.0 / 12.0;

    // Set the amount to accelerate by
    dAccel.x = 10;
    dAccel.y = 3;

    // Set the number of updates a jump can last
    maxJumpTime = 8;

    // Set the maximum distance it can fall before taking damage (-1 for
    // infinity)
    maxFallDistance = 300;

    // Set the sprite name
    // Todo: have a race
    sprite = "bunny.png";
    spriteWidth = 38;
    spriteHeight = 32;

    // Range for placing and mining tiles
    tileReachUp = 6;
    tileReachDown = 4;
    tileReachSideways = 5;

    // Initialize stats
    health.maxStat = 100;
    health.totalWidth = 190;
    health.h = 8;
    health.fill();
    stamina.maxStat = 100;
    stamina.totalWidth = 190;
    stamina.h = 8;
    stamina.fill();
    mana.maxStat = 100; 
    mana.totalWidth = 190;
    mana.h = 8;
    mana.fill();  

    // We don't need to set the statbar colors because the entity constructor
    // already did.

    // Set the location of the inventory
    inventory.x = hotbar.xStart;
    // The position of the bottom of the hotbar
    inventory.y = hotbar.yStart + hotbar.frame.height + hotbar.offsetDown;
    // The size of the gap between the hotbar and the inventory
    inventory.y += 16;
    trash.x = inventory.x;
    // The 32s here are for Inventory::squareSprite.width and height,
    // which haven't been initialized yet. TODO: fix
    trash.x += (inventory.getWidth() - 1) * 32;
    trash.y = inventory.y + 4;
    trash.y += inventory.getHeight() * 32;

    // Have them update where their clickboxes are
    inventory.updateClickBoxes();
    trash.updateClickBoxes();

    // Start with nothing in the mouse slot
    mouseSlot = NULL;

    // Have starting items
    inventory.pickup(ItemMaker::makeItem(ItemType::HEALTH_POTION));
    inventory.pickup(ItemMaker::makeItem(ItemType::DIRT));

    isInventoryOpen = false;
}

// Switch whether the inventory is open or closed
void Player::toggleInventory() {
    isInventoryOpen = !isInventoryOpen;
}

// Whether a place is within range for placing tiles
// x is distance from the player horizontally, in tiles.
// y is distance from the player in tiles, with positive being above the 
// player. bonus is the bonus range from possible unknown curcumstances
// (e.g. this type of tile can be placed farther away, or this pickax has
// better range).
bool Player::canReach(int x, int y, int bonus) {
    // If not in range in the x direction, return false
    if (abs(x) > tileReachSideways + bonus) {
        return false;
    }

    // If too high, return false
    if (y > tileReachUp + bonus) {
        return false;
    }

    // If too low, return false
    if (-1 * y > tileReachDown + bonus) {
        return false;
    }

    // Otherwise, it's within reach
    return true;

}

