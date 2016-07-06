#include <cassert>
#include <iostream>
#include <SDL2/SDL_image.h>
#include "WindowHandler.hh"

using namespace std;

// Return a rectangle in world coordinates with x and y at the center
SDL_Rect WindowHandler::findCamera(int x, int y) {
    SDL_Rect camera;
    camera.x = x - screenWidth / 2;
    camera.y = y - screenHeight / 2;
    camera.w = screenWidth;
    camera.h = screenHeight;

    // Adjust the camera if necessary, so that it's entirely on the screen
    camera.x = min(camera.x, worldWidth - screenWidth);
    camera.y = min(camera.y, worldHeight - screenHeight);
    camera.x = max(0, camera.x);
    camera.y = max(0, camera.y);

    // If, God forbid, the map is smaller than the camera, shrink the camera
    camera.w = min(camera.w, worldWidth - TILE_HEIGHT);
    camera.h = min(camera.h, worldHeight - TILE_WIDTH);

    return camera;
}

// Convert a rectangle from world coordinates to screen coordinates
SDL_Rect WindowHandler::convertRect(SDL_Rect rect, int x, int y) {
    SDL_Rect camera = findCamera(x, y);
    int xScreen = rect.x - camera.x;
    int yScreen = camera.y + screenHeight - rect.y;
    SDL_Rect screenRect = { xScreen, yScreen, rect.w, rect.h };

    return screenRect;
}

// Constructor
WindowHandler::WindowHandler(int screenWidth, int screenHeight, 
                    int mapWidth, int mapHeight, int tileWidth, int tileHeight) 
    : screenWidth(screenWidth), screenHeight(screenHeight), 
        TILE_WIDTH(tileWidth), TILE_HEIGHT(tileHeight), TILE_PATH("content/"), 
        worldWidth(TILE_WIDTH * mapWidth),
        worldHeight(TILE_HEIGHT * mapHeight) {
    window = NULL;
    screenSurface = NULL;
    renderer = NULL;

    // Set the 2D vector of rects for the tiles
    resize(screenWidth, screenHeight);
}

void WindowHandler::setMinimized(bool minimized) {
    isMinimized = minimized;
}

void WindowHandler::resize(int width, int height) {
    screenWidth = width;
    screenHeight = height;

    // Set the 2D vector of rects for the tiles
    // Access goes [x][y]
    tileRects.resize((screenWidth / TILE_WIDTH) + 1);
    for (unsigned i = 0; i < tileRects.size(); i++) {
        tileRects[i].resize((screenHeight / TILE_HEIGHT) + 1);
    }
}

// Start up the window
bool WindowHandler::init() {
    bool success = true;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        cerr << "SDL could not initialize. SDL_Error: ";
        cerr << SDL_GetError() << endl;
        success = false;
    }
    else {
        // Create window
        window = SDL_CreateWindow("Hello whirreled!", SDL_WINDOWPOS_UNDEFINED,
                    SDL_WINDOWPOS_UNDEFINED, screenWidth, screenHeight,
                    SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (window == NULL) {
            cerr << "Window could not be created. SDL_Error: ";
            cerr << SDL_GetError() << endl;
            success = false;
        }
        else {
            // Create a renderer for the window
            renderer = SDL_CreateRenderer(window, -1, 
                                            SDL_RENDERER_ACCELERATED);

            // Fall back to a software renderer if necessary
            if (renderer == NULL) {
                cerr << "Hardware-accelerated renderer could not be created. ";
                cerr << "SDL Error: " << endl << SDL_GetError() << endl;
                cerr << "Falling back to software renderer." << endl;
                renderer = SDL_CreateRenderer(window, -1, 
                                            SDL_RENDERER_SOFTWARE);
            }
            if (renderer == NULL) {
                cerr << "Software-accelerated renderer could not be created. ";
                cerr << "SDL Error: " << endl << SDL_GetError() << endl;
                success = false;
            }
            else {
                // Initialize renderer draw color
                SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);

                // Initialize PNG loading
                int imgFlags = IMG_INIT_PNG;
                if (!(IMG_Init(imgFlags) & imgFlags)) {
                    cerr << "SDL_image could not initialize. SDL_image Error: ";
                    cerr << endl << IMG_GetError() << endl;
                    success = false;
                }
            }
        }
    }
    // Assume the winodw is not minmized
    isMinimized = false;

    return success;
}

// Load test.bmp to surface, return true if successful
bool WindowHandler::loadMedia(vector<Tile *> &pointers) {
    bool success = true;

    // Load the textures for tiles
    loadTiles(pointers);

    return success;
}

// Load an image onto a surface and convert it to match the screen
SDL_Texture *WindowHandler::loadTexture(const string &name) {
    // Declare variables and load a surface
    SDL_Surface *surface = IMG_Load(name.c_str());
    SDL_Texture *texture = NULL;
    if (surface == NULL) {
        cerr << "Unable to load image " << name << endl;
        cerr << "SDLError: " << SDL_GetError() << endl;
    }
    // Make a texture
    else {
        // Convert the surface to a texture
        texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture == NULL) {
            cerr << "Failed to crate texture from " <<  name << endl;
            cerr << "SDL Error: " << SDL_GetError() << endl;
        }

        // Get rid of the surface
        SDL_FreeSurface(surface);
    }

    // Add it to the list of textures so it can be deallocated later
    textures.push_back(texture);

    return texture;
}

// Load a texture for each tile
bool WindowHandler::loadTiles(vector<Tile *> &pointers) {
    bool success = true;
    for (unsigned i = 0; i < pointers.size(); i++) {
        string name = TILE_PATH + pointers[i] -> sprite;
        loadTexture(name);
        assert(textures.size() != 0);
        if (textures.back() == NULL) {
            success = false;
        }

        // Set the tile's texture to the loaded texture
        assert(pointers[i] -> texture == NULL);
        pointers[i] -> texture = textures.back();
    }

    return success;
}

// Render everything the map holds information about
// x and y are the center of view of the camera, in pixels, 
// where y = 0 at the bottom
// If either value puts the camera past the end of the map, it will be fixed
void WindowHandler::renderMap(const Map &m, unsigned x, unsigned y) {
    // Make sure the renerer draw color is set to white
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);

    // Find the camera
    SDL_Rect camera = findCamera(x, y);

    // Rectangle to draw to
    SDL_Rect *rectTo;

    // Iterate through every tile at least partially within the camera
    for (int i = 0; i < (camera.w / TILE_WIDTH) + 1; i++) {
        int xRectTo = i * TILE_WIDTH - (camera.x % TILE_WIDTH);
        for (int j = 0; j < (camera.h / TILE_HEIGHT) + 1; j++) {
            // Set the rectangle to the correct one in the vector2D
            rectTo = &tileRects[i][j];

            rectTo -> w = TILE_WIDTH;
            rectTo -> x = xRectTo;

            // Remember that screen y == 0 at the top but world y == 0 at 
            // the bottom. Here j == 0 at the top of the screen.
            // We're not using convertRect because that doesn't align them
            // wit hthe tile grid.
            rectTo -> h = TILE_HEIGHT;
            rectTo -> y = (camera.h + camera.y) % TILE_HEIGHT;
            rectTo -> y += (j - 1) * TILE_HEIGHT;

            // Render the tile
            int xTile = (camera.x / TILE_WIDTH) + i;
            int yTile = (camera.y + camera.h) / TILE_HEIGHT - j;
            // But only if it's a tile that exists on the map
            if (0 <= xTile && xTile < worldWidth / TILE_WIDTH 
                && 0 <= yTile && yTile < worldHeight / TILE_HEIGHT) { 
                Tile *tile = m.getTile(xTile, yTile);
                SDL_RenderCopy(renderer, tile -> texture, NULL, rectTo);
            }
            else {
                cerr << "Tried to render the tile at " << xTile;
                cerr << ", " << yTile << endl;
                cerr << "camera.x is " << camera.x << ", camera.y is ";
                cerr << camera.y << ", camera.w is " << camera.w;
                cerr << ", camera.h is " << camera.h << endl;
            }
        }
    }
}

// Update the screen
void WindowHandler::update(const Map &map, const vector<Movable *> &movables) {
    // Clear the screen
    SDL_RenderClear(renderer);

    // Put a black rectangle in the background
    SDL_Rect fillRect = { 0, 0, screenWidth, screenHeight };
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderFillRect(renderer, &fillRect);

    // Only draw stuff if it isn't minimized
    if (!isMinimized) {

        // Put stuff on it
        // movables[0] should be the player
        renderMap(map, movables[0] -> x, movables[0] -> y);

        // Update the screen
        SDL_RenderPresent(renderer);
    }
}

// Close the window, clean up, and exit SDL
void WindowHandler::close() {
    // Deallocate any textures
    for(unsigned i = 0; i < textures.size(); i++) {
        // If I were being a good programmer I'd make the pointers
        // point to NULL
        SDL_DestroyTexture(textures[i]);
    }

    // Destroy window
    SDL_DestroyRenderer(renderer);
    renderer = NULL;
    SDL_DestroyWindow(window);
    window = NULL;

    // Quit SDL subsystems
    IMG_Quit();
    SDL_Quit();
}