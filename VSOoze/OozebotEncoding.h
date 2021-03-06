#ifndef OOZEBOT_ENCODING_H
#define OOZEBOT_ENCODING_H

#include <vector>
#include "cppSim.h"

enum OozebotExpressionType {
    boxDeclaration, // combination of springs and masses - one size mass (kg), and spring config for all springs (k, a, b, c)
    layAndMove, // Building block commands to form creation instructions
    // Strings together a sequence of layAndMove commands with a "thickness" that's 1D, 2D, or 3D with radius provided
    layBlockAndMoveCursor, // Takes in a lay and move index to iterator as well as "anchor" direction to grow out of + thickness
    symmetryScope, // creation commands within this scope are duplicated flipped along the x/y/z asis
};

enum OozebotDirection {
    up,
    down,
    left,
    right,
    forward,
    back,
};

enum OozebotAxis {
    xAxis,
    yAxis,
    zAxis,
    noAxis,
};

class OozebotExpression {
public:
    OozebotExpressionType expressionType;

    float kg; // 0.001 - 0.1
    float uk; // 0.02 - 1
    float us; // 0.02 - 1
    float k; // 500 - 10,000
    float a; // expressed as a ratio of l0's natural length 0.5-1.5
    float b; // 0 - 0.6, often 0
    float c; // 0 - 2pi
    int blockIdx; // which block to lay
    OozebotDirection direction;
    OozebotAxis scopeAxis;
    int layAndMoveIdx;
    int radius;
    OozebotAxis thicknessIgnoreAxis;
    // For extremities we grow out of the surface block reached from the center point moving in steps of these magnitude
    double anchorX;
    double anchorY;
    double anchorZ;
};

struct SimInputs {
    std::vector<Point> points;
    std::vector<Spring> springs;
    std::vector<FlexPreset> springPresets;
    double length;
};

class OozebotEncoding {
public:
    double fitness; // Depends on objective - might be net displacement
    double lengthAdj; // Fitness normalized for maximum dimension cross section
    double globalTimeInterval; // 2 - 10
    unsigned long int id;

    static OozebotEncoding mate(OozebotEncoding &parent1, OozebotEncoding &parent2);

    static SimInputs inputsFromEncoding(OozebotEncoding &encoding);

    // Sync on the handle to get the result
    static void evaluate(OozebotEncoding &encoding, double duration);

    static OozebotEncoding randomEncoding();

    // DSL that generates the SimInputs
    std::vector<OozebotExpression> boxCommands;
    std::vector<std::vector<OozebotExpression>> layAndMoveCommands;
    OozebotExpression bodyCommand;
    std::vector<OozebotExpression> growthCommands;
};

OozebotEncoding mutate(OozebotEncoding encoding);

unsigned long int newGlobalID();

// Returns true if the first encoding dominates the second, false otherwise
inline bool dominates(OozebotEncoding firstEncoding, OozebotEncoding secondEncoding) {
    return firstEncoding.fitness >= secondEncoding.fitness && firstEncoding.lengthAdj >= secondEncoding.lengthAdj;
}

#endif
