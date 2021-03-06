#include <iostream>
#include <fstream> // To read and write files
#include <cassert>
#include <ctime> // To seed the random number generator
#include <cstdlib> // For randomness
#include <cmath> // Because pi and exponentiation
#include <algorithm> // For max and min
#include "Mapgen.hh"
#include "version.hh"

using namespace std;
using namespace noise;
using json = nlohmann::json;

void Mapgen::setSize(int x, int y) {
    map.setHeight(y);
    map.setWidth(x);
    map.biomes.resize(map.biomesWide * map.biomesHigh);
    assert(map.tiles == nullptr);
    map.tiles = new SpaceInfo[map.width * map.height];
    cylinderScale.SetXScale((map.width / 2.0) / M_PI);
    cylinderScale.SetZScale(cylinderScale.GetXScale());
}

void Mapgen::generateEarth(CreateState *state, mutex *m) {
    /* Set height and width, and use them to make a tile array. */
    setSize(2048 * 3, 2048);

    baseHeight = map.height * 0.8;
    seaLevel = map.height * 0.72;
    seafloorLevel = map.height * 0.5;

    /* Inform on status. */
    m -> lock();
    *state = CreateState::GENERATING_BIOMES;
    m -> unlock();

    /* Some constants to use in the perlin moise. */
    const int octaves = 2;
    const double persistence = 0.2;
    const double scale = 0.0014;

    /* Make a Perlin noise module for temperature, humidity, and magicalness,
    for use in determining biome. */
    module::Perlin baseTemperature;
    baseTemperature.SetOctaveCount(octaves);
    baseTemperature.SetPersistence(persistence);
    baseTemperature.SetSeed(rand());
    module::ScalePoint scaledTemperature;
    scaledTemperature.SetScale(scale);
    scaledTemperature.SetSourceModule(0, baseTemperature);
    module::Turbulence finalTemperature;
    finalTemperature.SetSourceModule(0, scaledTemperature);
    finalTemperature.SetFrequency(scale);
    

    /* Same, but for humidity. */
    module::Perlin baseHumidity;
    baseHumidity.SetOctaveCount(octaves);
    baseHumidity.SetPersistence(persistence);
    baseTemperature.SetSeed(rand());
    module::ScalePoint scaledHumidity;
    scaledHumidity.SetScale(scale);
    scaledHumidity.SetSourceModule(0, baseHumidity);
    module::Turbulence finalHumidity;
    finalHumidity.SetSourceModule(0, scaledHumidity);
    finalHumidity.SetFrequency(scale);

    const int nsamples = 10000;
    vector<double> tempPercentiles;
    vector<double> humidityPercentiles;
    for (unsigned int i = 0; i < biomeData.size() - 1; i++) {
        double percentile = (i + 1) / (double)biomeData.size();
        tempPercentiles.push_back(getPercentile(percentile, finalTemperature,
            nsamples));
        humidityPercentiles.push_back(getPercentile(percentile, finalHumidity,
            nsamples));
    }

    /* Use the temperature and humidity to get the actual biomes. */
    for (int i = 0; i < map.biomesWide; i++) {
        for (int j = 0; j < map.biomesHigh; j++) {
            int x = i * BIOME_SIZE;
            int y = j * BIOME_SIZE;
            double temperature = getCylinderValue(x, y, finalTemperature);
            double humidity = getCylinderValue(x, y, finalHumidity);
            BiomeInfo info;
            info.biome = getBaseBiome(temperature, humidity, tempPercentiles,
                humidityPercentiles);
            map.setBiome(i, j, info);
        }
    }

    /* Inform on status. */
    m -> lock();
    *state = CreateState::GENERATING_TERRAIN;
    m -> unlock();

    /* Now that biomes are set, make a cave system. */
    module::RidgedMulti baseCaves;
    baseCaves.SetSeed(rand());
    module::Turbulence turbulentCaves;
    turbulentCaves.SetSourceModule(0, baseCaves);
    module::ScalePoint finalCaves;
    finalCaves.SetSourceModule(0, turbulentCaves);
    finalCaves.SetScale(0.005);
    finalCaves.SetYScale(2 * finalCaves.GetYScale());
    double caveBoundary = getPercentile(0.75, finalCaves, 10000);

    /* Add a system of tunnels to hopefully connect the caves. */
    module::RidgedMulti baseTunnels;
    baseTunnels.SetSeed(rand());
    module::Turbulence turbulentTunnels;
    turbulentTunnels.SetSourceModule(0, baseTunnels);
    module::ScalePoint finalTunnels;
    finalTunnels.SetSourceModule(0, baseTunnels);
    finalTunnels.SetScale(0.0011);
    finalTunnels.SetYScale(3 * finalTunnels.GetYScale());
    double tunnelBoundary = getPercentile(0.85, finalTunnels, 10000);
    /* A perlin noise to use for getting the surface. */
    module::Perlin baseSurface;
    baseSurface.SetSeed(rand());
    module::Turbulence turbulentSurface;
    turbulentSurface.SetSourceModule(0, baseSurface);
    module::ScalePoint finalSurface;
    finalSurface.SetSourceModule(0, turbulentSurface);
    const double hillScale = 0.001;
    const double steepness = 50000.0 * hillScale;
    finalSurface.SetScale(hillScale);

    const int cavernHeight = map.height * 0.5;
    double caveLimit = getPercentile(0.125, finalSurface, 10000);
    caveLimit -= getPercentile(0.875, finalCaves, 10000);
    double cavernLimit = getPercentile(0.05, finalSurface, 10000);
    cavernLimit -= getPercentile(0.95, finalCaves, 10000);
    const int shoreline = map.width * 0.25;
    const int abyss = map.width * 0.35;

    /* Wetness as in whether there is actually water there right now. */
    module::Perlin baseWetness;
    baseWetness.SetSeed(rand());
    module::Turbulence turbulentWetness;
    turbulentWetness.SetSourceModule(0, baseWetness);
    module::ScalePoint scaledWetness;
    scaledWetness.SetSourceModule(0, turbulentWetness);
    scaledWetness.SetScale(0.01);
    module::ScaleBias biasedWetness;
    biasedWetness.SetSourceModule(0, scaledWetness);
    biasedWetness.SetScale(1.5);
    module::Add finalWetness;
    finalWetness.SetSourceModule(0, biasedWetness);
    finalWetness.SetSourceModule(1, finalHumidity);

    double waterLimit = getPercentile(0.85, finalWetness, 10000);

    /* Just for testing, base the foreground tile on biome. */
    for (int i = 0; i < map.width; i++) {
        for (int j = 0; j < map.height; j++) {
            TileType tileType = TileType::STONE;

            /* Find the sky and make it empty. */
            double surface = getCylinderValue(i, j, finalSurface);
            /* Add the ocean. */
            surface += ocean(i, j, steepness, shoreline, abyss);

            if (surface > 0) {
                tileType = TileType::EMPTY;
            }
 
            /* Set the caves to be empty. */ 
            double cave = getCylinderValue(i, j, finalCaves);
            if (cave > caveBoundary 
                    && surface - cave < caveLimit) {
                tileType = TileType::EMPTY;
            }

            /* Set the tunnels to be empty. */
            double tunnel = getCylinderValue(i, j, finalTunnels);
            double tunnelHeight = (j - cavernHeight) / steepness / 2.0;
            if (tunnel > tunnelBoundary
                        && max(surface, tunnelHeight + surface / 2.0) 
                     - tunnel < cavernLimit) {
                tileType = TileType::EMPTY;
            }

            /* Add water instead of air to moist underground areas. */
            if (tileType == TileType::EMPTY && surface <= 0
                    && getCylinderValue(i, j, finalWetness) > waterLimit) {
                tileType = TileType::WATER;
            }

            map.setTileType(i, j, MapLayer::FOREGROUND, tileType);
        }
    }

    m -> lock();
    *state = CreateState::FELSIC;
    m -> unlock();
    setFelsic();

    /* Inform on status. */
    m -> lock();
    *state = CreateState::STUFF;
    m -> unlock();

    /* Put water on the surface. */
    map.savePPM(MapLayer::FOREGROUND, "wunsettled");

    /* Inform on status. */
    m -> lock();
    *state = CreateState::SETTLING_WATER;
    m -> unlock();

    settleWater();
    removeWater(20);
    settleWater();

    /* Fill the ocean. */
    for (int i = 0; i < map.width; i++) {
        /* magic number 30 is bigger than random surface variations but small
        enough to still be below any floating islands. */
        for (int j = baseHeight + 30; j >= 0; j--) {
            if (map.getTileType(i, j, MapLayer::FOREGROUND) != TileType::EMPTY) {
                break;
            }
            if (j < seaLevel) {
                map.setTileType(i, j, MapLayer::FOREGROUND, TileType::WATER);
            }
        }
    }

    /* When done setting non-boulders and before setting boulders, have
    all the tiles use a random sprite. */
    map.randomizeSprites();
}

void Mapgen::generateTest() {
    setSize(128, 64);
    for (int i = 0; i < map.width; i++) {
        for (int j = 0; j < map.height / 2; j++) {
            map.setTile(i, j, MapLayer::FOREGROUND, TileType::SANDSTONE);
        }
    }

    map.randomizeSprites();
}

double Mapgen::getCylinderValue(int x, int y, const module::Module &values) {
    cylinderScale.SetSourceModule(0, values);
    return cylinder.GetValue(x * 360.0 / map.width, y);
}

double Mapgen::getPercentile(double percentile, module::Module &values, 
        int samples) {
    /* Seed rand() with the module's seed so this always gives the same result
    for the same module. */
    vector<double> results;
    for (int i = 0; i < samples; i++) {
        double value = values.GetValue(rand(), rand(), rand());
        /* Find where in the vector to insert it. */
        int j;
        for (j = 0; j < i; j++) {
            if (value <= results[j]) {
                results.insert(results.begin() + j, value);
                break;
            }
        }
        /* If it wasn't added, it's the biggest number and should be added to
        the end. */
        if (j == i) {
            results.push_back(value);
        }
        assert((int)results.size() == i + 1);
    }

    int index = (int)(percentile * (double)samples);
    assert(index <= samples);
    assert(0 <= index);
    return results[index];
}

BiomeType Mapgen::getBaseBiome(double temperature, double humidity, 
        vector<double> tempPercentiles, vector<double> humidityPercentiles) {
    assert(tempPercentiles.size() == biomeData.size() - 1);
    assert(humidityPercentiles.size() == biomeData.size() - 1);

    /* Find which percentile it's in, and use our biomeData vector to figure
    out what biome that means. */
    int h = 0;
    int t = 0;
    while (t < (int)tempPercentiles.size() 
            && temperature > tempPercentiles[t]) {
        t++;
    }
    while (h < (int)humidityPercentiles.size()
            && humidity > humidityPercentiles[h]) {
        h++;
    }

    return (BiomeType)biomeData[t][h];
}


double Mapgen::ocean(int x, int y, double steepness, int shoreline, 
        int abyss) {
    double surface = (y - baseHeight) / steepness;
    double quadratic = 20 * ((x - map.width / 2.0) 
            * (x - map.width / 2.0)) / (map.width * map.width);
    double linear = 5 * abs(map.width / 2.0 - x) / map.width;
    int depth = (baseHeight - seafloorLevel) / steepness;
    if (abs(map.width / 2.0 - x) > abyss) {
        surface += depth + linear;
    }
    else if (abs(map.width / 2.0 - x) > shoreline) {
        double dist = (abs(map.width / 2.0 - x) - shoreline) 
                        / (abyss - shoreline);
        double interp = dist < 0.5? pow(0.5 - dist, 1 / 3.0) 
            : -1 * pow(dist - 0.5, 1 / 3.0);
        interp /= 2 * 0.7937; //cube root of 0.5
        interp += 0.5;
        surface += (1 - interp) * (depth + linear);
        surface += interp * quadratic;
    }
    else {
        surface += quadratic;
    }
    return surface;
}

void Mapgen::setFelsic() {
    /* Perlin noise for felsic / mafic gradient. */
    module::Perlin baseFelsic;
    baseFelsic.SetSeed(rand());
    module::Turbulence turbulentFelsic;
    turbulentFelsic.SetSourceModule(0, baseFelsic);
    module::ScalePoint finalFelsic;
    finalFelsic.SetScale(0.001);
    finalFelsic.SetSourceModule(0, turbulentFelsic);
    double basaltLimit = getPercentile(0.25, finalFelsic, 10000);
    double graniteLimit = getPercentile(0.75, finalFelsic, 10000);
    double peridotLimit = getPercentile(0.05, finalFelsic, 10000);

    for (int i = 0; i < map.width; i++) {
        int surface = map.height;
        for (int j = map.height - 1; j >= 0; j--) {
            /* Figure out the felsic - mafic value of the rock. */
            TileType tileType = map.getTileType(i, j, MapLayer::FOREGROUND);
            if (tileType == TileType::STONE) {
            // Alternately:
            // if (tileType != TileType::EMPTY) {
                if (surface == map.height) {
                    // NOTE: floating islands could disrupt this
                    surface = j;
                }

                double felsic = getCylinderValue(i, j, finalFelsic);
                double interp = 0;
                /* Adjust so that continental plates tend to be made of 
                granite, while oceanic plates tend to be made of basalt, 
                and the upper mantle is peridotite. */

                if (seafloorLevel - j > surface - seafloorLevel) {
                    double dist = surface > seafloorLevel? 
                        2 * seafloorLevel - surface : surface;
                    interp = abs((dist - j) / dist);
                    felsic -= abs(peridotLimit + basaltLimit) / 2.0 + interp;
                }
                else if (seafloorLevel - j == surface - seafloorLevel) {
                    // pass
                }
                else {
                    double dist = 2 * (surface - seafloorLevel);
                    interp = abs((dist - (surface - j)) / dist);
                    felsic += abs(graniteLimit) / 2.0 + 0.2 * interp;
                }

                if (felsic < peridotLimit && interp - 0.7 > 0.25 * felsic) {
                    tileType = TileType::PERIDOTITE;
                }
                else if (felsic < basaltLimit) {
                    tileType = TileType::BASALT;
                }
                else if (felsic > graniteLimit) {
                    tileType = TileType::GRANITE;
                }

                map.setTileType(i, j, MapLayer::FOREGROUND, tileType);
            }
        }
    }
}

void Mapgen::moveTileFast(int x1, int y1, int x2, int y2, MapLayer layer) {
    x1 = map.wrapX(x1);
    x2 = map.wrapX(x2);
    map.setTileType(x2, y2, layer, map.getTileType(x1, y1, layer));
    map.setTileType(x1, y1, layer, TileType::EMPTY);
}

int Mapgen::findFall(int direction, int x, int y, MapLayer layer) {
    assert(direction == 1 || direction == -1);
    /* Can't move down if already the bottom. */
    assert(y > 0);
    int current = x;
    while (current != map.wrapX(x - direction)) {
        /* Check if it can go down. */
        if (map.getTileType(current, y-1, layer) == TileType::EMPTY) {
            break;
        }
        TileType inTheWay = map.getTileType(current, y, layer);
        if (inTheWay != TileType::EMPTY && current != x) {
            /* Skip to the end of the loop to indicate failure,
            so I can use break to indicate success. */
            current = map.wrapX(x - direction);
            continue;
        }
        current += direction;
        current = map.wrapX(current);
    }
    return current;
}

void Mapgen::moveWater(int x, int y) {
    /* Make sure the tile being moved is actually water. */
    assert(map.getTileType(x, y, MapLayer::FOREGROUND) == TileType::WATER);

    /* If this is the bottom layer, it can't fall. */
    if (y == 0) {
        return;
    }

    /* First try moving it in the -x direction to move it down,
    then in the +x. */
    int fall = findFall(-1, x, y, MapLayer::FOREGROUND);
    /* If the while loop ended with current != i + 1, then
    current is where the water should be moved. Otherwise, try 
    the other direction. */
    if (fall == map.wrapX(x + 1)) {
        fall = findFall(1, x, y, MapLayer::FOREGROUND);
        /* If it can't fall that way either, move on. */
        if (fall == map.wrapX(x - 1)) {
            return;
        }
    }

    /* Otherwise, move the tile. */
    TileType below = map.getTileType(fall, y - 1, MapLayer::FOREGROUND);
    assert(below == TileType::EMPTY);
    int lowest = y - 1;
    /* See how far down it can be moved. */
    while (below == TileType::EMPTY && lowest > 0) {
        below = map.getTileType(fall, lowest - 1, MapLayer::FOREGROUND);
        if (below != TileType::EMPTY) {
            break;
        }
        lowest--;
    }
    moveTileFast(x, y, fall, lowest, MapLayer::FOREGROUND);
    /* Try to move the water again. */
    moveWater(fall, lowest);

    /* And this water block may have been in the way of the water block to the
    left of it falling, so let's try moving that again. */
    assert(map.getTileType(x, y, MapLayer::FOREGROUND) == TileType::EMPTY);
    if (map.getTileType(x-1, y, MapLayer::FOREGROUND) == TileType::WATER) {
        moveWater(map.wrapX(x - 1), y);
    }
}

void Mapgen::fillWater(int fillDepth) {
    /* First, place the water on top and let it fall. */
    for (int i = 0; i < map.width; i++) {
        for (int j = map.height - fillDepth; j < map.height; j++) {
            map.setTileType(i, j, MapLayer::FOREGROUND, TileType::WATER);
        }
    }
}

void Mapgen::settleWater() {
    /* Make it flow sideways. First iterate over the top layer, trying to
    move each one down a level if it can, then the next layer, and so on. */
    /* j > 0 not j >= 0 because we're looking at the level below. */
    for (int j = 1; j < map.height; j++) {
        for (int i = 0; i < map.width; i++) {
            if (map.getTileType(i, j, MapLayer::FOREGROUND)
                     == TileType::WATER) {
                moveWater(i, j);
            }
        }
    }
}

void Mapgen::removeWater(int removeDepth) {
    /* Remove the top removeDepth layers from each puddle. */
    for (int i = 0; i < map.width; i++) {
        int toRemove = removeDepth;
        int j = map.height - 1;
        while (toRemove > 0 && j >= 0) {
            TileType tile = map.getTileType(i, j, MapLayer::FOREGROUND);
            /* If there's water there, remove it. */
            if (tile == TileType::WATER) {
                map.setTileType(i, j, MapLayer::FOREGROUND, TileType::EMPTY);
                toRemove--;
            }
            /* If there's a solid tile, stop. */
            else if (tile != TileType::EMPTY) {
                break;
            }
            j--;
        }
    }
}

Mapgen::Mapgen(std::string path) : map(path) { }

void Mapgen::generate(std::string filename, WorldType worldType, 
        std::string path, CreateState *state, mutex *m) {
    /* Seed the random number generators. */
    map.seed = time(NULL);
    srand(map.seed);
    generator.seed(map.seed);

    /* Set the cylinder to get it's values from the scaled module. Of course,
    the scaled module will need to get its values from somewhere, too. */
    cylinder.SetModule(cylinderScale);

    /* Set the biome data vector. TODO: not hardcode filename? */
    std::ifstream infile(path + "content/biomes.json");
    if (!infile) {
        std::cout << "Can't open " << path + "content/biomes.json" << "\n";
    }
    json j = json::parse(infile);
    biomeData = j["biomes"].get<std::vector<std::vector<int>>>();

    /* Run the appropriate function. */
    switch(worldType) {
        case WorldType::TEST : 
            generateTest();
            break;
        case WorldType::SMOLTEST :
            break;
        case WorldType::EARTH :
            generateEarth(state, m);
            break;
        default :
            cerr << "Maybe I'll implement that later." << endl;
    }

    map.spawn.x = map.width / 2;
    /* I should be careful to make sure there are never cloud cities or 
    floating islands or whatever directly above the spawn point, so the
    player doesn't die of fall damage every time they respawn. */
    map.spawn.y = map.height * 0.9;
    *state = CreateState::SAVING;
    map.save(filename);
    /* TODO: remove when done testing. */
    map.savePPM(MapLayer::FOREGROUND, filename);
    map.saveBiomePPM(filename);
    m -> lock();
    *state = CreateState::DONE;
    m -> unlock();
}


