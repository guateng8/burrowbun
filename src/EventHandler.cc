#include <cassert>
#include <iostream>
#include "EventHandler.hh"

// Include things that were forward declared in the header
#include "WindowHandler.hh"
#include "Player.hh"
#include "World.hh"
#include "Hotbar.hh"
#include "Button.hh"
#include "Menu.hh"
#include "DroppedItem.hh"

using namespace std;

// Tell whether a scancode is in a vector
bool EventHandler::isIn(SDL_Scancode key, vector<SDL_Scancode> keys) {
    for (unsigned i = 0; i < keys.size(); i++) {
        if (keys[i] == key) {
            return true;
        }
    }

    // None of the keys in the vector match the key
    return false;
}

// Tell whether a vector has a key that's being held down
bool EventHandler::isHeld(const Uint8 *state, vector<SDL_Scancode> keys) {
    for (unsigned int i = 0; i < keys.size(); i++) {
        if (state[keys[i]]) {
            return true;
        }
    }

    return false;
}

// Update a single mouse box
bool EventHandler::updateBox(MouseBox &box) {
    // Mouse coordinates, relative to the window
    int x;
    int y;
    // Find the mosue coordinates
    SDL_GetMouseState(&x, &y);

    bool answer = false;
    // Note that MouseBox.contains(x, y) also sets mouseBox.containsMouse
    // to the appropriate value
    if (box.contains(x, y)) {
        answer = true;
        box.event.x = x;
        box.event.y = y;
        // If it was a button press in the box, fill in the
        // appropriate fields
        bool clickedLeft = isLeftButtonDown || (leftClicks != 0);
        bool clickedRight = isRightButtonDown || (rightClicks != 0);
        if (clickedLeft || clickedRight) {
            box.wasClicked = true;
            box.event.type = SDL_MOUSEBUTTONDOWN;
            // If left and right buttons clicked simultaneously, it's left
            if (clickedLeft) {
                box.event.button = SDL_BUTTON_LEFT;
                box.isHeld = wasLeftButtonDown;
            }
            else {
                box.event.button = SDL_BUTTON_RIGHT;
                box.isHeld = wasRightButtonDown;
            }
        }
    }
    // And the mouseBox is responsible for making wasClicked false again,
    // so we don't want to do that here
    return answer;
}

// Change the bool values of a MouseBox vector so they know whether they were
// clicked
bool EventHandler::updateMouseBoxes(vector<MouseBox> &mouseBoxes) {
    // Whether the mouse is in a box
    bool answer = false;

    for (unsigned int i = 0; i < mouseBoxes.size(); i++) {
        answer = answer || updateBox(mouseBoxes[i]);
    }

    return answer;

}

// Update the mouseboxes in an inventory
bool EventHandler::updateInventoryClickBoxes(Inventory &inventory) {
    bool answer = false;
    // Update each row, and return true if updating any row returns true
    for (int i = 0; i < inventory.getHeight(); i++) {
        answer = answer || updateMouseBoxes(inventory.clickBoxes[i]);
    }

    return answer;
}


// Public methods

// Constructor
EventHandler::EventHandler() {
    // Assume the player starts off not moving
    left = false;
    right = false;
    up = false;
    down = false;
    jump = false;

    isJumping = false;
    hasJumped = false;

    isLeftButtonDown = false;
    isRightButtonDown = false;
    wasLeftButtonDown = false;
    wasRightButtonDown = false;

    leftClicks = 0;
    rightClicks = 0;

    move = 0;

    // There might be a less repetitive way to do this.
    keySettings.leftKeys.push_back(SDL_SCANCODE_LEFT);
    keySettings.leftKeys.push_back(SDL_SCANCODE_A);
    keySettings.rightKeys.push_back(SDL_SCANCODE_RIGHT);
    keySettings.rightKeys.push_back(SDL_SCANCODE_D);
    keySettings.upKeys.push_back(SDL_SCANCODE_UP);
    keySettings.upKeys.push_back(SDL_SCANCODE_W);
    keySettings.downKeys.push_back(SDL_SCANCODE_DOWN);
    keySettings.downKeys.push_back(SDL_SCANCODE_S);
    keySettings.jumpKeys.push_back(SDL_SCANCODE_SPACE);
    keySettings.jumpKeys.push_back(SDL_SCANCODE_KP_SPACE);
    // Keys to open the inventory and whatever else opens along with it
    keySettings.inventoryKeys.push_back(SDL_SCANCODE_I);
    keySettings.inventoryKeys.push_back(SDL_SCANCODE_C);
    /* Keys to toss items. */
    keySettings.tossKeys.push_back(SDL_SCANCODE_T);
    // And each of 24 keys to select a hotbar slot
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_1);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_2);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_3);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_4);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_5);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_6);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_7);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_8);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_9);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_0);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_MINUS);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_EQUALS);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_F1);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_F2);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_F3);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_F4);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_F5);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_F6);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_F7);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_F8);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_F9);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_F10);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_F11);
    keySettings.hotbarKeys.push_back(SDL_SCANCODE_F12);
}

// Access functions
KeySettings EventHandler::getKeySettings() {
    return keySettings;
}

void EventHandler::setKeySettings(KeySettings &newSettings) {
    keySettings = newSettings;
}

// Handle window events
void EventHandler::windowEvent(const SDL_Event &event, bool &isFocused,
                                    WindowHandler &window) {
    switch(event.window.event) {
        case SDL_WINDOWEVENT_FOCUS_GAINED:
            isFocused = true;
            break;
        case SDL_WINDOWEVENT_FOCUS_LOST:
            isFocused = false;
            break;
        case SDL_WINDOWEVENT_MINIMIZED:
            window.setMinimized(true);
            break;
        case SDL_WINDOWEVENT_RESTORED:
            window.setMinimized(false);
            break;
        case SDL_WINDOWEVENT_MAXIMIZED:
            window.setMinimized(false);
            // Purposely no break
        case SDL_WINDOWEVENT_RESIZED:
        case SDL_WINDOWEVENT_SIZE_CHANGED:
            window.resize(event.window.data1, event.window.data2);
            break;
        default:
            // cerr << "Recieved unsupported window event." << endl;
            break;
    }
}

// Update the state of the mouse
void EventHandler::mouseEvent(const SDL_Event &event) {
    // Ignore mouse wheel and mouse motion events
    if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        // What state to set the button to
        bool which;
        which = (event.type == SDL_MOUSEBUTTONDOWN);
        // Which button to set
        if (event.button.button == SDL_BUTTON_LEFT) {
            // Record a button down and up happening in the same frame
            leftClicks += (int)(!which && isLeftButtonDown 
                && !wasLeftButtonDown);
            isLeftButtonDown = which;
        }
        else if (event.button.button == SDL_BUTTON_RIGHT) {
            rightClicks += (int)(!which && isRightButtonDown 
                && !wasRightButtonDown);
            isRightButtonDown = which;
        }
    }
}

// Do whatever should be done when a mouse event happens
void EventHandler::useMouse(Player &player, World &world) {
    // Whether the mouse has clicked on something
    bool isMouseUsed;

    // Tell the hotbar and inventories whether they were clicked
    isMouseUsed = updateInventoryClickBoxes(player.hotbar);
    // Only send the inventory clicks if it's open
    if (player.isInventoryOpen) {
        // Update the inventory clickboxes and set isMosueUsed to true if
        // it clicked on any of them
        isMouseUsed = isMouseUsed 
            || updateInventoryClickBoxes(player.inventory)
            || updateInventoryClickBoxes(player.trash);
    }
    // If the mouse hasn't clicked on any part of the UI, use the item it
    // is holding, if any
    if (!isMouseUsed) {
        InputType type = InputType::NONE;
        if ((isLeftButtonDown && !wasLeftButtonDown) || leftClicks != 0) {
            type = InputType::LEFT_BUTTON_PRESSED;
        }
        else if ((isRightButtonDown && !wasRightButtonDown) 
                || rightClicks != 0) {
            type = InputType::RIGHT_BUTTON_PRESSED;
        }
        else if (isLeftButtonDown) {
            type = InputType::LEFT_BUTTON_HELD;
        }
        else if (isRightButtonDown) {
            type = InputType::RIGHT_BUTTON_HELD;
        }
        // Where the mouse clicked, in world coordinates
        int x, y;
        SDL_GetMouseState(&x, &y);
        /* Note that this will give a negative answer if it wraps around with
        the player near 0 and the click near the width of the map. */
        x = player.getRect().x + x - player.screenX;
        y = player.getRect().y - y + player.screenY;
        // Have the player figure out whether to use an item, and which one
        player.useAction(type, x, y, world);
    }
    // All done, set clicks to 0 for next time
    leftClicks = 0;
    rightClicks = 0;
}

// Do whatever should be done when key presses or releases happen
void EventHandler::keyEvent(const SDL_Event &event, Player &player, 
        vector<DroppedItem *> &drops) { 
    SDL_Scancode key = event.key.keysym.scancode;

    // Here we should handle keys which don't need to be held down to work.
    if (event.type == SDL_KEYUP) {
        // Pass
    }
    else if (isIn(key, keySettings.inventoryKeys)) {
        player.toggleInventory();
    }
    else if (isIn(key, keySettings.tossKeys)) {
        player.toss(drops);
    }
    else if (isIn(key, keySettings.hotbarKeys)) {
        // Select the appropriate slot in the hotbar
        // This vector actually has the order matter, so you can't map more
        // than one key to each hotbar slot
        for (unsigned int i = 0; i < keySettings.hotbarKeys.size(); i++) {
            if (key == keySettings.hotbarKeys[i]) {
                player.hotbar.select(i);
            }
        }
    }
}

// Do stuff that depends on keys being held down.
void EventHandler::updateKeys(const Uint8 *state) {
    // Initialize
    left = false;
    right = false;
    up = false;
    down = false;
    jump = false;

    // Try to tell whether keys that matter are up or down
    if (isHeld(state, keySettings.leftKeys)) {
        left = true;
    }
    if (isHeld(state, keySettings.rightKeys)) {
        right = true;
    }
    if (isHeld(state, keySettings.upKeys)) {
        up = true;
    }
    if (isHeld(state, keySettings.downKeys)) {
        down = true;
    }
    if (isHeld(state, keySettings.jumpKeys)) {
        jump = true;
    }
    else {
        isJumping = false;
        hasJumped = false;
    }
}

// Change the player's acceleration
void EventHandler::updatePlayer(Player &player) {
    // update the player's accelleration
    movable::Point newAccel;
    newAccel.x = 0;
    newAccel.y = 0;

    /* Accelerate in the direction the keys are saying we should. */
    if (right) {
        newAccel.x += player.getDAccel().x;
    }
    if (left) {
        newAccel.x -= player.getDAccel().x;
    }
    if (jump && (player.timeOffGround <= player.maxJumpTime
            || player.maxJumpTime == -1)
            && (isJumping == hasJumped)) {
        newAccel.y += player.getDAccel().y;
        isJumping = true;
        hasJumped = true;
    }
    else {
        isJumping = false;
    }

    // TODO: handle these separately, so the player can't fly
    if (up) {
        newAccel.y += player.getDAccel().y;
    }
    if (down) {
        player.collidePlatforms = false;
    }
    else {
        player.collidePlatforms = true;
    }

    // Change the player's acceleration
    player.setAccel(newAccel);
}

void EventHandler::updateMenu(Menu &menu) {
    for (unsigned int i = 0; i < menu.buttons.size(); i++) {
        updateBox(menu.buttons[i]);
    }
    // TODO: put this code elsewhere
    // All done, set clicks to 0 for next time
    leftClicks = 0;
    rightClicks = 0;
    // Get ready for next update
    wasLeftButtonDown = isLeftButtonDown;
    wasRightButtonDown = isRightButtonDown;
}

// Do all the things that need to be done every update
void EventHandler::update(World &world) {
    // Use keys that need to be held down
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    updateKeys(state);

    // Use the mouse if a button is being held down
    useMouse(world.player, world);
    // Get ready for next update
    wasLeftButtonDown = isLeftButtonDown;
    wasRightButtonDown = isRightButtonDown;

    // Tell the player what they're trying to do
    updatePlayer(world.player);

}


