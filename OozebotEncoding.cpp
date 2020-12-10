#define _USE_MATH_DEFINES
#include <cmath>

#include <math.h>
#include <algorithm>
#include <utility>
#include <map>
#include <atomic>
#include <random>
#include <time.h>
#include <thread>

#include "OozebotEncoding.h"

const int kNumBoxes = 6;
const int kMaxLayAndMoveSequences = 6;
const int kMaxLayAndMoveLength = 30;
const int kMaxGrowthCommands = 8;
const int kMaxRadius = 8;

std::atomic<unsigned long int> GlobalId(1);

bool springSortFunction(OozebotExpression a, OozebotExpression b) {
    return (a.b > b.b);
}

struct Coordinate
{
    int x, y, z;
    Coordinate(int x, int y, int z) : x(x), y(y), z(z) {}
    bool operator<(const Coordinate& rhs) const
        {
            if (x<rhs.x) return true;
            if (x==rhs.x)
            {
                if (y<rhs.y) return true;
                if (y==rhs.y) return z<rhs.z;
            }
            return false;
        }
};

#if defined (_MSC_VER)  // Visual studio
    #define thread_local __declspec( thread )
#elif defined (__GCC__) // GCC
    #define thread_local __thread
#endif

int randomInRange(const int & min, const int & max) {
    static thread_local std::mt19937 generator = std::mt19937(clock() + std::hash<std::thread::id>()(std::this_thread::get_id()));
    std::uniform_int_distribution<int> distribution(min, max);
    return distribution(generator);
}

double randFloat() {
    return (double) randomInRange(0, 10000000) / 10000000;
}

OozebotEncoding OozebotEncoding::randomEncoding() {
    std::vector<OozebotExpression> boxCommands;
    boxCommands.reserve(kNumBoxes);
    for (int i = 0; i < kNumBoxes; i++) {
        OozebotExpression boxCreationExpression;
        boxCreationExpression.expressionType = boxDeclaration;
        boxCreationExpression.kg = 0.001 + randFloat() * 0.099;
        double r = randFloat(); // 0 to 1
        boxCreationExpression.k = 500.0 + r * 9500.0;
        r = randFloat(); // 0 to 1
        if (r < 0.5) { // half the time have it be 1
            boxCreationExpression.a = 1;
        } else {
            r = randFloat(); // 0 to 1
            boxCreationExpression.a = 0.5 + r;
        }
        r = randFloat(); // 0 to 1
        if (r < 0.2) { // have it not expand/contract
            boxCreationExpression.b = 0;
        } else {
            r = randFloat(); // 0 to 1
            boxCreationExpression.b = r * 0.66;
        }
        r = randFloat(); // 0 to 1
        boxCreationExpression.c = r * 2 * M_PI;
        boxCommands.push_back(boxCreationExpression);
    }
    std::sort(boxCommands.begin(), boxCommands.end(), springSortFunction);

    std::vector<std::vector<OozebotExpression>> layAndMoveSequences;
    layAndMoveSequences.reserve(kMaxLayAndMoveSequences);
    for (int i = 0; i < kMaxLayAndMoveSequences; i++) {
        std::vector<OozebotExpression> sequence;
        sequence.reserve(kMaxLayAndMoveLength);
        for (int j = 0; j < kMaxLayAndMoveLength; j++) {
            OozebotExpression layAndMoveExpression;
            layAndMoveExpression.expressionType = layAndMove;
            // Add bias to duplicate direction and type
            if (j > 0) {
                double r = randFloat(); // 0 to 1
                if (r < 0.6) { // Half the time we keep the same direction
                    layAndMoveExpression.direction = sequence[j - 1].direction;
                } else {
                    layAndMoveExpression.direction = static_cast<OozebotDirection>(randomInRange(0, 5));
                }
                r = randFloat(); // 0 to 1
                if (r < 0.6) { // half the time we keep the same block type
                    layAndMoveExpression.blockIdx = sequence[j - 1].blockIdx;
                } else {
                    layAndMoveExpression.blockIdx = randomInRange(0, kNumBoxes - 1);
                }
            } else {
                layAndMoveExpression.direction = static_cast<OozebotDirection>(randomInRange(0, 5));
                layAndMoveExpression.blockIdx = randomInRange(0, kNumBoxes - 1);
            }
            sequence.push_back(layAndMoveExpression);

            double r = randFloat(); // 0 to 1
            if (r < 0.02) { // Don't always have to be full length, end early 2% of the time for each iteration
                break;
            }
        }
        layAndMoveSequences.push_back(sequence);
    }

    OozebotExpression bodyCommand;
    bodyCommand.expressionType = layBlockAndMoveCursor;
    bodyCommand.layAndMoveIdx = randomInRange(0, kMaxLayAndMoveSequences - 1);
    bodyCommand.radius = randomInRange(0, kMaxRadius - 1);
    bodyCommand.thicknessIgnoreAxis = static_cast<OozebotAxis>(randomInRange(0, 3)); // sometimes make body 2D

    std::vector<OozebotExpression> growthCommands;
    for (int i = 0; i < kMaxGrowthCommands; i++) {
        double r = randFloat(); // 0 to 1
        OozebotExpression growthExpression;
        if (r < 0.4) {
            growthExpression.expressionType = symmetryScope;
            growthExpression.scopeAxis = static_cast<OozebotAxis>(randomInRange(0, 2));
        } else {
            growthExpression.expressionType = layBlockAndMoveCursor;
            growthExpression.layAndMoveIdx = randomInRange(0, kMaxLayAndMoveSequences - 1);
            growthExpression.radius = randomInRange(0, kMaxRadius - 1);
            growthExpression.thicknessIgnoreAxis = static_cast<OozebotAxis>(randomInRange(0, 3)); // sometimes make body 2D
            growthExpression.anchorX = randomInRange(-5, 5);
            growthExpression.anchorY = randomInRange(-5, 5);
            growthExpression.anchorZ = randomInRange(-5, 5);
            while (growthExpression.anchorX == 0 && growthExpression.anchorY == 0 && growthExpression.anchorZ == 0) {
                growthExpression.anchorX = randomInRange(-5, 5);
                growthExpression.anchorY = randomInRange(-5, 5);
                growthExpression.anchorZ = randomInRange(-5, 5);
            }
        }
        growthCommands.push_back(growthExpression);
        r = randFloat();
    }

    OozebotEncoding encoding;
    double r = randFloat(); // 0 to 1
    encoding.globalTimeInterval = 1.0 + r * 9.0;
    encoding.lengthAdj = 0;
    encoding.id = GlobalId.fetch_add(1, std::memory_order_relaxed);
    encoding.boxCommands = boxCommands;
    encoding.layAndMoveCommands = layAndMoveSequences;
    encoding.bodyCommand = bodyCommand;
    encoding.growthCommands = growthCommands;
    return encoding;
}

// Maybe change linkage in the future - could not split at mid or tie boxes to lay and move sequences
OozebotEncoding OozebotEncoding::mate(OozebotEncoding parent1, OozebotEncoding parent2) {
    OozebotEncoding child;
    child.boxCommands = {};
    int boxSplitI = randomInRange(0, kNumBoxes - 1);
    int boxSplitJ = randomInRange(0, kNumBoxes - 1);
    if (boxSplitJ == boxSplitI) {
        boxSplitJ = randomInRange(0, kNumBoxes - 1);
    }
    int ii = std::min(boxSplitI, boxSplitJ);
    int jj = std::max(boxSplitI, boxSplitJ);
    int i = 0;
    while (i < kNumBoxes) {
        if (i < ii || i > jj) {
            child.boxCommands.push_back(parent1.boxCommands[i]);
        } else {
            child.boxCommands.push_back(parent2.boxCommands[i]);
        }
        i++;
    }
    std::sort(child.boxCommands.begin(), child.boxCommands.end(), springSortFunction);

    child.layAndMoveCommands = {};
    boxSplitI = randomInRange(0, kMaxLayAndMoveSequences - 1);
    boxSplitJ = randomInRange(0, kMaxLayAndMoveSequences - 1);
    if (boxSplitJ == boxSplitI) {
        boxSplitJ = randomInRange(0, kMaxLayAndMoveSequences - 1);
    }
    ii = std::min(boxSplitI, boxSplitJ);
    jj = std::max(boxSplitI, boxSplitJ);
    i = 0;
    while (i < kMaxLayAndMoveSequences) {
        if (i < ii || i > jj) {
            child.layAndMoveCommands.push_back(parent1.layAndMoveCommands[i]);
        } else {
            child.layAndMoveCommands.push_back(parent2.layAndMoveCommands[i]);
        }
        i++;
    }
    child.bodyCommand = parent1.bodyCommand;

    child.growthCommands = {};
    boxSplitI = randomInRange(0, kMaxGrowthCommands - 1);
    boxSplitJ = randomInRange(0, kMaxGrowthCommands - 1);
    if (boxSplitJ == boxSplitI) {
        boxSplitJ = randomInRange(0, kMaxGrowthCommands - 1);
    }
    ii = std::min(boxSplitI, boxSplitJ);
    jj = std::max(boxSplitI, boxSplitJ);
    i = 0;
    while (i < kMaxGrowthCommands) {
        if (i < ii || i > jj) {
            child.growthCommands.push_back(parent1.growthCommands[i]);
        } else {
            child.growthCommands.push_back(parent2.growthCommands[i]);
        }
        i++;
    }
    child.id = GlobalId++;
    child.globalTimeInterval = parent1.globalTimeInterval;
    return child;
}

OozebotEncoding mutate(OozebotEncoding encoding) {
    // Mutate either a box command, lay and move command, body, or growth
    int r = randomInRange(0, 99);
    if (r < 5) { // 5% of the time do the body
        r = randomInRange(0, 99);
        if (r < 10) {
            encoding.bodyCommand.layAndMoveIdx = randomInRange(0, kMaxLayAndMoveSequences - 1);
        } else if (r < 20) {
            encoding.bodyCommand.thicknessIgnoreAxis = static_cast<OozebotAxis>(randomInRange(0, 3)); // sometimes make body 2D
        } else {
            int newRadius = encoding.bodyCommand.radius + (randomInRange(0, 1) ? 1 : -1);
            encoding.bodyCommand.radius = std::max(std::min(newRadius, 0), kMaxRadius);
        }
    } else if (r < 8) {
        double seed = randFloat() - 0.5; // -0.5 to 0.5
        double interval = encoding.globalTimeInterval + seed;
        encoding.globalTimeInterval = std::min(std::max(interval, 1.0), 10.0);
    } else if (r < 30) {
        int index = randomInRange(0, (int) (encoding.boxCommands.size() - 1));
        double seed = randFloat() - 0.5; // -0.5 to 0.5
        r = randomInRange(0, 4);
        if (r == 0) {
            double k = encoding.boxCommands[index].k + seed * 50.0;
            encoding.boxCommands[index].k = std::min(std::max(k, 500.0), 10000.0);
        } else if (r == 1) {
            double a = encoding.boxCommands[index].a + seed * 0.1;
            encoding.boxCommands[index].a = std::min(std::max(a, 0.5), 1.5);
        } else if (r == 2) {
            double b = encoding.boxCommands[index].b + seed * 0.05;
            encoding.boxCommands[index].b = std::min(std::max(b, 0.0), 0.66);
        } else if (r == 3) {
            double c = encoding.boxCommands[index].c + seed * 0.1;
            encoding.boxCommands[index].c = std::min(std::max(c, 0.0), 2 * M_PI);
        } else {
            double kg = encoding.boxCommands[index].kg + seed * 0.01;
            encoding.boxCommands[index].kg = std::min(std::max(kg, 0.001), 0.1);
        }
        std::sort(encoding.boxCommands.begin(), encoding.boxCommands.end(), springSortFunction);
    } else if (r < 60) {
        int index = randomInRange(0, (int) (encoding.layAndMoveCommands.size() - 1));
        int subIndex = randomInRange(0, (int) (encoding.layAndMoveCommands[index].size() - 1));
        encoding.layAndMoveCommands[index][subIndex].direction = static_cast<OozebotDirection>(randomInRange(0, 5));
        encoding.layAndMoveCommands[index][subIndex].blockIdx = randomInRange(0, kNumBoxes - 1);
    } else {
        int index = randomInRange(0, (int) (encoding.growthCommands.size() - 1));
        if (encoding.growthCommands[index].expressionType == symmetryScope) {
            encoding.growthCommands[index].scopeAxis = static_cast<OozebotAxis>(randomInRange(0, 2));
        } else {
            r = randomInRange(0, 99);
            if (r < 20) {
                encoding.growthCommands[index].layAndMoveIdx = randomInRange(0, kMaxLayAndMoveSequences - 1);
            } else if (r < 40) {
                encoding.growthCommands[index].radius = randomInRange(0, kMaxRadius - 1);
            } else if (r < 70) {
                encoding.growthCommands[index].thicknessIgnoreAxis = static_cast<OozebotAxis>(randomInRange(0, 3)); // sometimes make body 2D
            } else if (r < 80) {
                int newAnchor = encoding.growthCommands[index].anchorX + randomInRange(0, 1) ? -1 : 1;
                encoding.growthCommands[index].anchorX = std::max(std::min(newAnchor, -5), 5);
            } else if (r < 90) {
                int newAnchor = encoding.growthCommands[index].anchorY + randomInRange(0, 1) ? -1 : 1;
                encoding.growthCommands[index].anchorY = std::max(std::min(newAnchor, -5), 5);
            } else {
                int newAnchor = encoding.growthCommands[index].anchorZ + randomInRange(0, 1) ? -1 : 1;
                encoding.growthCommands[index].anchorZ = std::max(std::min(newAnchor, -5), 5);
            }
            while (encoding.growthCommands[index].anchorX == 0 && encoding.growthCommands[index].anchorY == 0 && encoding.growthCommands[index].anchorZ == 0) {
                encoding.growthCommands[index].anchorX = randomInRange(-5, 5);
                encoding.growthCommands[index].anchorY = randomInRange(-5, 5);
                encoding.growthCommands[index].anchorZ = randomInRange(-5, 5);
            }
        }
    }
    return encoding;
}

AsyncSimHandle OozebotEncoding::evaluate(OozebotEncoding encoding, int streamNum) {
    SimInputs inputs = OozebotEncoding::inputsFromEncoding(encoding);
    if (inputs.points.size() == 0) {
        return { {}, NULL, NULL, NULL};
    }
   
    return simulate(inputs.points, inputs.springs, inputs.springPresets, 6.0, encoding.globalTimeInterval, streamNum, inputs.length);
}

std::pair<double, double> OozebotEncoding::wait(AsyncSimHandle handle) {
    synchronize(handle);
    double mass = 0;
    double startX = 0;
    double startZ = 0;
    for (auto point : handle.points) {
        double pm = point.mass;
        startX += point.x * pm;
        startZ += point.z * pm;
        mass += pm;
    }
    startX = (startX / mass);
    startZ = (startZ / mass);
    resolveSim(handle);
    if (handle.points.size() == 0) {
        printf("failure\n");
        return {0, 0};
    }
    double endX = 0;
    double endZ = 0;
    for (const auto point : handle.points) {
        if (isnan(point.x) || isinf(point.x) || isnan(point.z) || isinf(point.z)) {
            printf("Solution has NaN or inf\n");
            return {0, 0};
        }
        double pm = point.mass;
        endX += point.x * pm;
        endX += point.z * pm;
    }
    endX = endX / mass;
    endZ = endZ / mass;
    const double deltaX = endX - startX;
    const double deltaZ = endZ - startZ;
    double fitness = sqrt(deltaX * deltaX + deltaZ * deltaZ) / handle.duration;
    return {fitness, fitness / std::max(1.5, handle.length) }; // Don't incentivize wee little robots - at least 15 length to avoid trivialities
}

void layBlockAtPosition(
    int x,
    int y,
    int z,
    std::vector<Point> &points,
    std::vector<Spring> &springs,
    std::map<Coordinate, int> &pointLocationToIndexMap,
    std::map<std::pair<int, int>, bool> &pointIndexHasSpring,
    OozebotExpression boxCommand,
    int idx) {
    std::vector<int> pointIndices;
    // first make the points
    for (int xi = x; xi < x + 2; xi++) {
        for (int yi = y; yi < y + 2; yi++) {
            for (int zi = z; zi < z + 2; zi++) {
                Coordinate p = {xi, yi, zi};
                if (pointLocationToIndexMap.find(p) == pointLocationToIndexMap.end()) {
                    // It wasn't already there so we add it
                    pointLocationToIndexMap[p] = (int) points.size();
                    Point p = {xi / 10.0f, yi / 10.0f, zi / 10.0f, 0, 0, 0, boxCommand.kg, 0, 0};
                    points.push_back(p);
                }
                pointIndices.push_back(pointLocationToIndexMap[p]);
            }
        }
    }
    // now make the springs
    for (int ii = 0; ii < pointIndices.size(); ii++) {
        for (int jj = ii + 1; jj < pointIndices.size(); jj++) {
            int first = std::min(pointIndices[ii], pointIndices[jj]);
            int second = std::max(pointIndices[ii], pointIndices[jj]);
            // always index from smaller to bigger so we don't have to double bookkeep
            if (pointIndexHasSpring.find({first, second}) == pointIndexHasSpring.end()) {
                pointIndexHasSpring[{first, second}] = true;
                Point p1 = points[first];
                Point p2 = points[second];
                float length = sqrt(pow(p1.x - p2.x, 2) + pow(p1.y - p2.y, 2) + pow(p1.z - p2.z, 2));
                Spring s = {boxCommand.k, first, second, length, p1.numSprings, p2.numSprings, idx};
                springs.push_back(s);
                points[first].numSprings += 1;
                points[second].numSprings += 1;
            }
        }
    }
}

int processExtremity(
    std::vector<OozebotExpression> &sequence,
    std::map<Coordinate, std::pair<int, int>> &boxIndexSpringType,
    int radius,
    OozebotAxis thicknessIgnoreAxis,
    int x,
    int y,
    int z,
    bool invertX,
    bool invertY,
    bool invertZ) {
    int globalMinY = 100;

    for (auto iter = sequence.begin(); iter != sequence.end(); ++iter) {
        OozebotExpression cmd = *iter;
        // First we "lay" the current block and ones around it, respecting proximity of radius
        int minX = x - radius;
        int maxX = x + radius;
        int minY = y - radius;
        int maxY = y + radius;
        int minZ = z - radius;
        int maxZ = z + radius;
        if (thicknessIgnoreAxis == xAxis) {
            minX = x;
            maxX = x;
        } else if (thicknessIgnoreAxis == yAxis) {
            minY = y;
            maxY = y;
        } else if (thicknessIgnoreAxis == zAxis) {
            minZ = z;
            maxZ = z;
        }

        for (int xi = minX; xi <= maxX; xi++) {
            for (int yi = minY; yi <= maxY; yi++) {
                int dist = abs(xi - x) + abs(yi - y);
                if (dist > radius) {
                    continue;
                }
                for (int zi = minZ; zi <= maxZ; zi++) {
                    int totalDist = dist + abs(zi - z);
                    if (totalDist > radius) {
                        continue;
                    }
                    Coordinate c = {xi, yi, zi};
                    if (boxIndexSpringType.find(c) == boxIndexSpringType.end() || boxIndexSpringType[c].first > totalDist) {
                        boxIndexSpringType[c] = {totalDist, cmd.blockIdx};
                        globalMinY = std::min(globalMinY, yi); // only update minY when we actually lay a block - otherwise we could end at a new low without laying
                    }
                }
            }
        }

        // Now we move
        OozebotDirection direction = cmd.direction;
        switch (direction) {
            case up:
                if (invertY == false) {
                    y += 1;
                } else {
                    y -= 1;
                }
                break;
            case down:
                if (invertY == false) {
                    y -= 1;
                } else {
                    y += 1;
                }
                break;
            case left:
                if (invertZ == false) {
                    z -= 1;
                } else {
                    z += 1;
                }
                break;
            case right:
                if (invertZ == false) {
                    z += 1;
                } else {
                    z -= 1;
                }
                break;
            case forward:
                if (invertX == false) {
                    x += 1;
                } else {
                    x -= 1;
                }
                break;
            case back:
                if (invertX == false) {
                    x -= 1;
                } else {
                    x += 1;
                }
                break;
        }
        x = std::max(std::min(x, 100), -100);
        y = std::max(std::min(y, 100), -100);
        z = std::max(std::min(z, 100), -100);
    }
    return globalMinY;
}

bool outOfBounds(std::map<Coordinate, std::pair<int, int>> &boxIndexSpringType, int x, int y, int z) {
    if (boxIndexSpringType.find({x, y, z}) == boxIndexSpringType.end()) {
        return true;
    }
    return false;
}

int processExtremityWithAnchor(
    std::vector<OozebotExpression> &sequence,
    std::map<Coordinate, std::pair<int, int>> &bodyIndexSpringType,
    std::map<Coordinate, std::pair<int, int>> &boxIndexSpringType,
    int radius,
    OozebotAxis thicknessIgnoreAxis,
    int anchorX,
    int anchorY,
    int anchorZ,
    bool invertX,
    bool invertY,
    bool invertZ) {
    int x = 0;
    int y = 0;
    int z = 0;
    int xi = 0;
    int yi = 0;
    int zi = 0;
    bool valid = true;
    bool tmpInvertX = !invertX;
    bool tmpInvertY = !invertY;
    bool tmpInvertZ = !invertZ;
    if (anchorX < 0) {
        anchorX = -anchorX;
        tmpInvertX = !invertX;
    }
    if (anchorY < 0) {
        anchorY = -anchorY;
        tmpInvertY = !invertY;
    }
    if (anchorZ < 0) {
        anchorZ = -anchorZ;
        tmpInvertZ = !invertZ;
    }
    while (true) {
        for (int i = 0; i < anchorX; i++) {
            xi += tmpInvertX ? -1 : 1;
            if (outOfBounds(bodyIndexSpringType, xi, yi, zi)) {
                valid = false;
                break;
            }
            x = xi;
        }
        if (!valid) {
            break;
        }
        for (int i = 0; i < anchorY; i++) {
            yi += tmpInvertY ? -1 : 1;
            if (outOfBounds(bodyIndexSpringType, xi, yi, zi)) {
                valid = false;
                break;
            }
            y = yi;
        }
        if (!valid) {
            break;
        }
        for (int i = 0; i < anchorZ; i++) {
            zi += tmpInvertZ ? -1 : 1;
            if (outOfBounds(bodyIndexSpringType, xi, yi, zi)) {
                valid = false;
                break;
            }
            z = zi;
        }
        if (!valid) {
            break;
        }
    }
    return processExtremity(
        sequence,
        boxIndexSpringType,
        radius,
        thicknessIgnoreAxis,
        x,
        y,
        z,
        invertX,
        invertY,
        invertZ);
}

SimInputs OozebotEncoding::inputsFromEncoding(OozebotEncoding encoding) {
    std::vector<Point> points;
    std::vector<Spring> springs;
    std::vector<FlexPreset> presets;

    for (auto it = encoding.boxCommands.begin(); it != encoding.boxCommands.end(); it++) {
        FlexPreset p = {(*it).a, (*it).b, (*it).c};
        presets.push_back(p);
    }

    // x -> y -> z -> (distance, box_index)
    std::map<Coordinate, std::pair<int, int>> bodyIndexSpringType;
    int minY = processExtremity(
        encoding.layAndMoveCommands[encoding.bodyCommand.layAndMoveIdx],
        bodyIndexSpringType,
        encoding.bodyCommand.radius,
        encoding.bodyCommand.thicknessIgnoreAxis,
        0,
        0,
        0,
        false,
        false,
        false);
    std::map<Coordinate, std::pair<int, int>> extremityIndexSpringType;
    bool invertX = false;
    bool invertY = false;
    bool invertZ = false;
    for (auto it = encoding.growthCommands.begin(); it != encoding.growthCommands.end(); it++) {
        OozebotExpression cmd = *it;
        if (cmd.expressionType == symmetryScope) {
            if (cmd.scopeAxis == xAxis) {
                invertX = true;
            } else if (cmd.scopeAxis == yAxis) {
                invertY = true;
            } else { // only these three values for this field
                invertZ = true;
            }
        } else { // it's a lay command
            // lay the anchors for each
            for (int flipX = 0; flipX < (invertX ? 1 : 2); flipX++) {
                for (int flipY = 0; flipY < (invertY ? 1 : 2); flipY++) {
                    for (int flipZ = 0; flipZ < (invertZ ? 1 : 2); flipZ++) {
                        minY = std::min(processExtremityWithAnchor(
                            encoding.layAndMoveCommands[cmd.layAndMoveIdx],
                            bodyIndexSpringType,
                            extremityIndexSpringType,
                            cmd.radius,
                            cmd.thicknessIgnoreAxis,
                            cmd.anchorX,
                            cmd.anchorY,
                            cmd.anchorZ,
                            !!flipX,
                            !!flipY,
                            !!flipZ), minY);
                    }
                }
            }
            // Reset these as they only apply to the next lay and move sequence
            invertX = false;
            invertY = false;
            invertZ = false;
        }
    }

    // All indexes are points in 3 space times 10 (position on tenth of a meter, index by integer)
    // Largest value is 100, smallest is -100 on each axis
    std::map<Coordinate, int> pointLocationToIndexMap;
    std::map<std::pair<int, int>, bool> pointIndexHasSpring;

    // Now we have priority of each material for each slot so we can lay the body
    for (auto iter = bodyIndexSpringType.begin(); iter != bodyIndexSpringType.end(); iter++) {
        int boxIndex = iter->second.second;
        layBlockAtPosition(
            (*iter).first.x,
            (*iter).first.y,
            (*iter).first.z,
            points,
            springs,
            pointLocationToIndexMap,
            pointIndexHasSpring,
            encoding.boxCommands[boxIndex],
            boxIndex);
    }
    // Now we lay the extremities
    for (auto iter = extremityIndexSpringType.begin(); iter != extremityIndexSpringType.end(); iter++) {
        int boxIndex = iter->second.second;
        layBlockAtPosition(
           (*iter).first.x,
           (*iter).first.y,
           (*iter).first.z,
            points,
            springs,
            pointLocationToIndexMap,
            pointIndexHasSpring,
            encoding.boxCommands[boxIndex],
            boxIndex);
    }

    float smallestX = 100;
    float largestX = -100;
    float smallestY = 100;
    float largestY = -100;
    float smallestZ = 100;
    float largestZ = -100;
    // ground robot on lowest point
    for (auto it = points.begin(); it != points.end(); ++it) {
        (*it).y -= (double(minY) / 10.0);
        smallestX = std::min((*it).x, smallestX);
        largestX = std::max((*it).x, largestX);
        smallestY = std::min((*it).y, smallestY);
        largestY = std::max((*it).y, largestY);
        smallestZ = std::min((*it).z, smallestZ);
        largestZ = std::max((*it).z, largestZ);
    }
    double length = (double) std::max(std::max(largestX - smallestX, largestY - smallestY), largestZ - smallestZ);

    return { points, springs, presets, length };
}
