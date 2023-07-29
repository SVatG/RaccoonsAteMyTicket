import numpy as np
import re
import sys
import struct

# TODO not working, in progress. but now that we have FBX export maybe not needed?

inFileName = sys.argv[1]
outFileName = sys.argv[2]

modelFile = open(inFileName, 'r') # TODO parameter lol

# Read vertices
vertexIndices = {}
vertexCount = 0
vertices = []
for line in modelFile:
    matches = re.match("([0-9]*): (.*)", line)
    if matches != None:
        vertexId = matches.groups()[0]
        vertexPos = list(map(lambda x: float(x), matches.groups()[1].split(" ")))
        
        vertices.append(vertexPos)
        vertexIndices[vertexId] = vertexCount
        vertexCount += 1
    else:
        if line == "RIG:\n":
            break

# Read rig
rigDict = {}
boneIndicesNames = {}
boneCount = 0
for line in modelFile:
    if line == "TRIANGLES:\n":
        break
    else:
        matches = re.match('Vertex ([0-9]*) Bone ([^:]*): (.+)', line)
        vertexIndex = vertexIndices[matches.groups()[0]]
        boneName = matches.groups()[1]
        boneWeight = float(matches.groups()[2])
        
        if not boneName in boneIndicesNames:
            boneIndicesNames[boneName] = boneCount
            boneCount += 1
        
        if not vertexIndex in rigDict:
            rigDict[vertexIndex] = {}
            
        rigDict[vertexIndex][boneIndicesNames[boneName]] = boneWeight
print(rigDict)

## Read tris
tris = []
for line in modelFile:
    if line == "ANIMATION:\n":
        break
    else:
        faceData = line.rstrip("\n").split(" ")

        for i in range(0, 3):
            vertexIndex = vertexIndices[faceData[i]]

            boneIndicesVert = [0, 0]
            boneWeightsVert = [0, 0]

            boneIndices = []
            boneWeights = []
            for boneIndex in range(boneCount):
                if vertexIndex in rigDict and boneIndex in rigDict[vertexIndex]:
                    boneIndices.append(boneIndex)
                    boneWeights.append(rigDict[vertexIndex][boneIndex])
            boneWeights, boneIndices = (list(t) for t in zip(*reversed(sorted(zip(boneWeights, boneIndices)))))
            for i in range(0, min(2, len(boneIndices))):
                boneIndicesVert[i] = boneIndices[i]
                boneWeightsVert[i] = boneWeights[i]

            denorm_vert = [
                vertices[vertexIndex], # position
                [float(x) for x in faceData[3 + 3 * i:6 + 3 * i]], # normal
                [float(x) for x in faceData[12 + 2 * i:14 + 2 * i]], # uv
                boneIndicesVert,
                boneWeightsVert
            ]
            tris.append(denorm_vert)


# Read animation
animationFrames = []
currentBoneIndex = -1
skipBone = False
for line in modelFile:
    if line[:5] == "Frame":
        animationFrames.append({})
        continue
        
    if line[:4] == "Bone":
        currentBone = line[5:].rstrip('\n')
        if currentBone in boneIndicesNames:
            currentBoneIndex = boneIndicesNames[currentBone]
            animationFrames[-1][currentBoneIndex] = []
            skipBone = False
        else:
            currentBoneIndex = -1
            skipBone = True
        continue
    
    if skipBone == False:
        splitNums = list(map(lambda x: float(x), line.rstrip(")>\n").split("(")[1].split(",")))
        animationFrames[-1][currentBoneIndex].extend(splitNums)


def swap_axes(matrix, axis1, axis2):
    axis1 += 1
    axis2 += 1
    swapped_matrix = matrix.copy()
    for i in range(axis1, len(matrix), 4):  # for each row starting from axis1
        corresponding_index_axis2 = (i // 4) * 4 + axis2  # corresponding index in axis2
        swapped_matrix[i], swapped_matrix[corresponding_index_axis2] = swapped_matrix[corresponding_index_axis2], swapped_matrix[i]  # swap elements
    return swapped_matrix


anim = []
for frame in animationFrames:
    animFrame = []
    for bone in sorted(boneIndicesNames.values()):
        print("FRAME", frame[bone])




        # WHAT
        # Extact 3x4 part and then move the homogenous translation over
        #boneData = frame[bone][12:15] + frame[bone][0:3] + frame[bone][4:7] + frame[bone][8:11]
        """        
        boneData[3] = 1.0
        boneData[4] = 0.0
        boneData[5] = 0.0
        boneData[6] = 0.0
        boneData[7] = 1.0
        boneData[8] = 0.0
        boneData[9] = 0.0
        boneData[10] = 0.0
        boneData[11] = 1.0
        """

        #rows = 4
        #cols = 3
        #boneData = [boneData[col + row*cols] for col in range(cols) for row in range(rows)]
        #boneData = swap_axes(boneData, 0, 2)
        #boneData =  boneData[8:12] + boneData[4:8] + boneData[0:4]
        #boneDataYZ = boneData
        # WHAT

        boneData = frame[bone][:4*3]
        boneData[3] = frame[bone][12] 
        boneData[7] = frame[bone][13]
        boneData[11] = frame[bone][14]

        animFrame.append(boneData)
        print("mat")
        print(boneData[0:4])
        print(boneData[4:8])
        print(boneData[8:12])
        print(boneData)

    anim.append(animFrame)

with open(outFileName, 'wb') as file:
    vertCount = len(tris)
    boneCount = len(anim[0]) if len(anim) > 0 else 0  # Assume all frames have the same number of bones
    frameCount = len(anim)

    # Write vertCount, boneCount, frameCount
    file.write(struct.pack('iii', vertCount, boneCount, frameCount))

    # Write vertex data
    for vertex in tris:
        pos, normal, uv, bones, boneWeights = vertex
        file.write(struct.pack('fff', *pos))  # Write position
        file.write(struct.pack('ff', *bones))  # Write bones
        file.write(struct.pack('ff', *boneWeights))  # Write boneWeights
        file.write(struct.pack('fff', *normal))  # Write normal
        file.write(struct.pack('ff', *uv))  # Write texcoord

    # Write animation frames
    for frame in anim:
        for bone in frame:
            file.write(struct.pack('f' * 12, *bone))
print("Wrote " + str(vertCount) + " vertices and " + str(frameCount) + " frames to " + outFileName + ".")
