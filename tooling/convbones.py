import numpy as np
import re
import sys

inFileName = sys.argv[1]
prefix = sys.argv[2]
outFileNameC = sys.argv[3]
outFileNameH = sys.argv[4]

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
        if line == "POLYGONS:\n":
            break
            
## Read tris
tris = []
for line in modelFile:
    if line == "RIG:\n":
        break
    else:
        faceData = line.rstrip("\n").split(" ")
        tris.append([[
            vertexIndices[faceData[0]],
            vertexIndices[faceData[1]],
            vertexIndices[faceData[2]],
        ], faceData[3:]])
        
# Read rig
rigDict = {}
boneIndicesNames = {}
boneCount = 0
for line in modelFile:
    if line == "ANIMATION:\n":
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

print(boneIndicesNames)

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
        
# Create vertex strings (including bone weights)
vertexStrings = []
maxBones = 0
for vertexIndex in range(len(vertices)):
    vertexString = ""
    vertexString += "{" + ", ".join(map(lambda x: str(x), vertices[vertexIndex])) + "}"
    
    boneIndices = []
    vertexWeights = []
    for boneIndex in range(boneCount):
        if vertexIndex in rigDict and boneIndex in rigDict[vertexIndex]:
            boneIndices.append(boneIndex)
            vertexWeights.append(rigDict[vertexIndex][boneIndex])
    
    maxBones = max(maxBones, len(boneIndices))
    if len(vertexWeights) != 0:
        vertexWeights, boneIndices = (list(t) for t in zip(*reversed(sorted(zip(vertexWeights, boneIndices)))))
        if len(boneIndices) > 4: # More than 4 bones would be a problem because then we can't stuff the weights into one vec4
            print("Dropping some bones with weights: " + str(vertexWeights[4:]))
    
    boneIndices = boneIndices[:1]
    if len(vertexWeights) != 0:
        vertexWeights = vertexWeights[:4] + [0] * (4 - len(vertexWeights))
    else:
        vertexWeights = [1, 0, 0, 0]
    
    boneIndices = boneIndices[:1] #+ [0] * (4 - len(boneIndices))
    if len(boneIndices) == 0:
        boneIndices = [0]

    vertexWeights = vertexWeights[:4] + [0] * (4 - len(vertexWeights))
    # vertexWeights = list(np.array(vertexWeights) / np.sum(np.array(vertexWeights)))
    boneIndices = list(np.array(boneIndices) * 3) # Precalc offset so the shader saves a few muls
    
    vertexString += ", " + ", ".join(map(lambda x: str(x), boneIndices)) + ""
    #vertexString += ", {" + ", ".join(map(lambda x: str(x), vertexWeights)) + "}"

    vertexStrings.append(vertexString)
  
# Create the tri string
faceVertexStrings = []
for vertIndices, faceData in tris:
    for i, idx in enumerate(vertIndices):
        faceVertexStrings.append("    { " + vertexStrings[idx] + ", {" + ", ".join(faceData[i * 3:(i+1)*3]) + "}, {" + ", ".join(faceData[9 + i * 4:9 + (i+1)*4]) + "} }")

if maxBones > 0:
    # Animation frames
    animationFrameStrings = []
    for frame in animationFrames:
        frameString = "    {\n"
        for bone in sorted(boneIndicesNames.values()):
            # Extact 3x4 part and then move the homogenous translation over
            boneData = frame[bone][:4*3]
            #boneData[3] = frame[bone][12] 
            #boneData[7] = frame[bone][13]
            #boneData[11] = frame[bone][14]
            frameString += "        { " + ", ".join(map(str, boneData)) + "},\n"
        frameString += "    }"
        animationFrameStrings.append(frameString)

with open(outFileNameC, 'w') as f:
    f.write("#include \"Tools.h\"\n\n")
    f.write("vertex_rigged " + prefix + "Verts[" + str(len(faceVertexStrings)) + "] = {\n")
    f.write(",\n".join(faceVertexStrings))
    f.write("\n};\n\n")
    
    if maxBones > 0:
        f.write("float " + prefix + "Anim[" + str(len(animationFrames)) + "][" + str(len(boneIndicesNames)) + "][12] = {\n")
        f.write(",\n".join(animationFrameStrings))
        f.write("\n};\n")
    
with open(outFileNameH, 'w') as f:
    f.write("#ifndef " + prefix.upper() + "\n")
    f.write("#define " + prefix.upper() + "\n\n")
    f.write("#define " + prefix + "NumVerts " + str(len(faceVertexStrings)) + "\n")
    f.write("extern vertex_rigged " + prefix + "Verts[" + str(len(faceVertexStrings)) + "];\n\n")
    
    if maxBones > 0:
        f.write("#define " + prefix + "NumFrames " + str(len(animationFrames)) + "\n")
        f.write("extern float " + prefix + "Anim[" + str(len(animationFrames)) + "][" + str(len(boneIndicesNames)) + "][12];\n\n")
    f.write("#endif\n")
    
