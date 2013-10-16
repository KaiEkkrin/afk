#!/usr/bin/python3

# This script interprets the output of AFK when compiled with AFK_SHAPE_ENUM_DEBUG
# and generates a report.

import fileinput
import re

# These regular expressions are used to parse the ASED debug output.
def makeCellSubRe(prefix):
    return 'Cell\((?P<{0}X>-?\d+), (?P<{0}Y>-?\d+), (?P<{0}Z>-?\d+), scale (?P<{0}S>-?\d+)\)'.format(prefix)

def makeKeyedCellSubRe(prefix):
    return 'Cell\((?P<{0}X>-?\d+), (?P<{0}Y>-?\d+), (?P<{0}Z>-?\d+), scale (?P<{0}S>-?\d+), key (?P<{0}K>-?\d+)\)'.format(prefix)

frameRe = re.compile('Now computing frame Frame (?P<number>[0-9]+)')
entEnqRe = re.compile('ASED: Enqueued entity: worldCell={0}, entity counter=(?P<counter>[0-9]+)'.format(makeCellSubRe('wc')))

shapeEnqRe = re.compile('ASED: Shape cell {0} of entity: worldCell={1}, entity counter=(?P<counter>[0-9]+) (?P<status>\w+)'.format(makeKeyedCellSubRe('sc'), makeCellSubRe('wc')))

# These functions make standard string forms of the various things
# parsed out, suitable for using as keys.
def keyifyEntity(entEnqMatch):
    return '{0}, {1}, {2}, {3}, counter {4}'.format(
        int(entEnqMatch.group('wcX')),
        int(entEnqMatch.group('wcY')),
        int(entEnqMatch.group('wcZ')),
        int(entEnqMatch.group('wcS')),
        int(entEnqMatch.group('counter')))

def keyifyShapeCell(shapeEnqMatch):
    return '{0}, {1}, {2}, {3}, key {4}'.format(
        int(shapeEnqMatch.group('scX')),
        int(shapeEnqMatch.group('scY')),
        int(shapeEnqMatch.group('scZ')),
        int(shapeEnqMatch.group('scS')),
        int(shapeEnqMatch.group('scK')))

# These dictionaries contain what I've found.  I want to collate the
# output by entity and by the shape cells within, and classify
# the number of times each one was found in various statuses.

frames = []

# keyified entity -> frame -> (nothing)
entEnq = {}

# keyified entity -> keyified shape cell -> frame -> status
shapeEnq = {}

def main():
    # Scan the input and build the dictionaries.
    currentFrame = 0
    for line in fileinput.input():
        frameReMatch = frameRe.match(line)
        if frameReMatch:
            currentFrame = int(frameReMatch.group('number'))
            #print('Found frame: {0}'.format(currentFrame))
            frames.append(currentFrame)

        entEnqMatch = entEnqRe.match(line)
        if entEnqMatch:
            #print('Found enqueued entity: world cell {0},{1},{2},{3}, counter {4}'.format(
            #    entEnqMatch.group('wcX'), entEnqMatch.group('wcY'), entEnqMatch.group('wcZ'), entEnqMatch.group('wcS'), entEnqMatch.group('counter')))
            entKey = keyifyEntity(entEnqMatch)
            if not entKey in entEnq:
                entEnq[entKey] = {}
            entEnq[entKey][currentFrame] = 'generated'

        shapeEnqMatch = shapeEnqRe.match(line)
        if shapeEnqMatch:
            entKey = keyifyEntity(shapeEnqMatch)
            shapeKey = keyifyShapeCell(shapeEnqMatch)
            status = shapeEnqMatch.group('status')
            #print('Found enqueued shape cell: {0} from entity {1} with status {2}'.format(shapeKey, entKey, status))
            if not entKey in shapeEnq:
                shapeEnq[entKey] = {}
            if not shapeKey in shapeEnq[entKey]:
                shapeEnq[entKey][shapeKey] = {}
            shapeEnq[entKey][shapeKey][currentFrame] = status

    # Next, go through the frames and compile totals of what happened.

    # keyified entity -> number of frames seen
    entEnqByFrame = {}
    for frame in frames:
        for (entKey, entVal) in entEnq.items():
            if frame in entVal:
                if not entKey in entEnqByFrame:
                    entEnqByFrame[entKey] = 0
                entEnqByFrame[entKey] += 1

    # keyified entity -> keyified shape cell -> status -> number of frames seen
    shapeEnqByFrame = {}
    for frame in frames:
        for (entKey, entVal) in shapeEnq.items():
            for (shapeKey, shapeVal) in entVal.items():
                if frame in shapeVal:
                    status = shapeVal[frame]
                    if not entKey in shapeEnqByFrame:
                        shapeEnqByFrame[entKey] = {}
                    if not shapeKey in shapeEnqByFrame[entKey]:
                        shapeEnqByFrame[entKey][shapeKey] = {}
                    if not status in shapeEnqByFrame[entKey][shapeKey]:
                        shapeEnqByFrame[entKey][shapeKey][status] = 0
                    shapeEnqByFrame[entKey][shapeKey][status] += 1

    # And finally, print a report.
    print('ENTITIES')
    print('========')
    print('')
    for (entKey, entVal) in entEnqByFrame.items():
        print('Entity {0}: Seen in {1} frames ({2}%)'.format(entKey, entVal, 100.0 * entVal / len(frames)))
    print('')

    print('SHAPES')
    print('======')
    print('')
    for (entKey, entVal) in shapeEnqByFrame.items():
        for (shapeKey, shapeVal) in entVal.items():
            reportByStatus = []
            for (status, count) in shapeVal.items():
                reportByStatus.append('{0} {1} times ({2}%)'.format(status, count, 100.0 * count / len(frames)))
            print('Entity {0}, shape cell {1}: {2}'.format(entKey, shapeKey, ', '.join(reportByStatus)))           
    print('')

if __name__ == "__main__":
    main()

