#include <stdio.h>
#include <stdlib.h>
#include "ufbx.h"

#define TOOLS_BASICS_ONLY
#include "Tools.h"

//#define DEBUG

fbxBasedObject loadFBXObject(const char* filename, const char* objectName) {
    // Output object
    fbxBasedObject objectNew;

    // Load data
    ufbx_load_opts opts = { 0 }; // Optional, pass NULL for defaults
    ufbx_error error; // Optional, pass NULL if you don't care about errors
    ufbx_scene *scene = ufbx_load_file(filename, &opts, &error);
    if (!scene) {
        fprintf(stderr, "Failed to load: %s\n", error.description.data);
        return objectNew;
    }

    // Go through all objects in the scene
    for (size_t i = 0; i < scene->nodes.count; i++) {
        ufbx_node *node = scene->nodes.data[i];
        if (node->is_root) continue;

        // Bail if not the right name
        #ifdef DEBUG
        printf("Found object %s\n", node->name.data);
        #endif
        if(objectName != NULL) {
            if(strncmp(node->name.data, objectName, 255) != 0) {
                #ifdef DEBUG
                printf("-> Have filter name and this is not it, skipping\n");
                #endif
                continue;
            }
        }
        
        #ifdef DEBUG
        printf("-> Loading\n");
        #endif

        // We can only load meshes
        if (node->mesh) {
            // Mesh and skin objects for convenience
            ufbx_mesh* mesh = node->mesh;
            ufbx_skin_deformer* skin = mesh->skin_deformers.data[0];

            // Alloc VBO data struct
            int totalVerts = mesh->faces.count * 3;
            objectNew.vbo = (vertex_rigged*)malloc(sizeof(vertex_rigged) * totalVerts);
            #ifdef DEBUG
            printf("Allocated %d verts\n", totalVerts);
            #endif
            objectNew.vertCount = totalVerts;

            // Go through all faces
            size_t setVertIdx = 0;
            for (size_t faceIdx = 0; faceIdx < mesh->faces.count; faceIdx++) {
                // We support only triangles, please triangulate on export Or Else
                if(mesh->faces.data[faceIdx].num_indices != 3) {
                    // Complain and skip
                    printf("Yikes! Non-tri face at %ld\n", faceIdx);
                    continue;
                }

                // Get the mesh data
                size_t faceFirstVertIdx = mesh->faces.data[faceIdx].index_begin;
                for (size_t vertIdx = faceFirstVertIdx; vertIdx < faceFirstVertIdx + 3; vertIdx++) {
                    #ifdef DEBUG
                    printf("Vertex %ld\n", vertIdx);
                    #endif

                    // Standard geometry data
                    ufbx_vec3 pos = ufbx_get_vertex_vec3(&mesh->vertex_position, vertIdx);
                    #ifdef DEBUG
                    printf("  * pos: %f %f %f\n", pos.x, pos.y, pos.z);
                    #endif
                    ufbx_vec3 normal = ufbx_get_vertex_vec3(&mesh->vertex_normal, vertIdx);
                    #ifdef DEBUG
                    printf("  * normal: %f %f %f\n", normal.x, normal.y, normal.z);
                    #endif
                    ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, vertIdx);
                    #ifdef DEBUG
                    printf("  * uv: %f %f\n", uv.x, uv.y);
                    #endif

                    // Them bones. Of which we support two. More are possible at increased cost.
                    ufbx_vec2 boneIdx;
                    ufbx_vec2 boneWgt;
                    
                    boneIdx.x = 0.0;
                    boneIdx.y = 0.0;
                    boneWgt.x = 0.0;
                    boneWgt.y = 0.0;

                    if (skin != NULL) {
                        ufbx_skin_vertex* vertSkinData = &skin->vertices.data[mesh->vertex_indices.data[vertIdx]];
                        int numBones = vertSkinData->num_weights;
                        #ifdef DEBUG
                        printf("  * got skin data, bones: %d\n", numBones);
                        #endif
                        if(numBones >= 1) {
                            boneIdx.x = skin->weights.data[vertSkinData->weight_begin].cluster_index;
                            boneWgt.x = skin->weights.data[vertSkinData->weight_begin].weight;
                            #ifdef DEBUG
                            printf("  * bone 0: %f %f\n", boneIdx.x, boneWgt.x);
                            #endif
                        }
                        if(numBones >= 2) {
                            boneIdx.y = skin->weights.data[vertSkinData->weight_begin + 1].cluster_index;
                            boneWgt.y = skin->weights.data[vertSkinData->weight_begin + 1].weight;
                            #ifdef DEBUG
                            printf("  * bone 1: %f %f\n", boneIdx.y, boneWgt.y);
                            #endif
                        }
                    }
                    else {
                        #ifdef DEBUG
                        printf("  * no skin data\n");
                        #endif
                    }

                    // Set the vertex, with y and z swapped and the original y negated because it's illegal for a coordinate system
                    // used by a 3D engine to be the same as the one used by a 3D modelling tool (sike, we're doing it in thte matrix now)
                    objectNew.vbo[setVertIdx].position[0] = pos.x;
                    objectNew.vbo[setVertIdx].position[1] = pos.y;
                    objectNew.vbo[setVertIdx].position[2] = pos.z;
                    objectNew.vbo[setVertIdx].bones[0] = boneIdx.x * 3.0;
                    objectNew.vbo[setVertIdx].bones[1] = boneIdx.y * 3.0;
                    objectNew.vbo[setVertIdx].boneWeights[0] = boneWgt.x;
                    objectNew.vbo[setVertIdx].boneWeights[1] = boneWgt.y;
                    objectNew.vbo[setVertIdx].normal[0] = normal.x;
                    objectNew.vbo[setVertIdx].normal[1] = normal.y;
                    objectNew.vbo[setVertIdx].normal[2] = normal.z;
                    objectNew.vbo[setVertIdx].texcoord[0] = uv.x;
                    objectNew.vbo[setVertIdx].texcoord[1] = uv.y;
                    setVertIdx += 1;
                }
            }
            
            #ifdef DEBUG
            printf("Loaded %ld verts\n", setVertIdx);
            #endif

            // Is there an animation?
            objectNew.frameCount = 0;
            if(scene->anim_stacks.count >= 1) {
                if(scene->anim_stacks.count > 1) {
                    printf("Warning: more than one animation stack, only the first will be used\n");
                }
                #ifdef DEBUG
                printf("Loading animation\n");
                #endif
                ufbx_anim_stack* anim_stack = scene->anim_stacks.data[0];

                // We sample at 60fps
                float frameDur = 1.0 / 60.0;
                size_t frameCount = (size_t)((anim_stack->anim.time_end - anim_stack->anim.time_begin) / frameDur + 0.5);
                size_t boneCount = skin->clusters.count;
                #ifdef DEBUG
                printf("  * frame count: %ld\n", frameCount);
                printf("  * bone count: %ld\n", boneCount);
                #endif

                // Alloc data for frames
                objectNew.frameCount = frameCount;
                objectNew.boneCount = boneCount;
                objectNew.animFrames = (float*)malloc(sizeof(float) * frameCount * boneCount * 12);

                // Copy every frame
                for(size_t frame = 0; frame < frameCount; frame++) {
                    // Get every bones transform for the frame
                    for(size_t boneIdx = 0; boneIdx < boneCount; boneIdx++) {
                        ufbx_skin_cluster* cluster = skin->clusters.data[boneIdx];
                        ufbx_node* bone = cluster->bone_node;
                        float frameTime = anim_stack->anim.time_begin + frame * frameDur;
                        ufbx_transform transform = ufbx_evaluate_transform(&anim_stack->anim, bone, frameTime);
                        ufbx_matrix transformMatLocal = ufbx_transform_to_matrix(&transform);
                        ufbx_matrix transformMat = ufbx_matrix_mul(&transformMatLocal, &cluster->geometry_to_bone);

                        #ifdef DEBUG
                        printf("%f %f %f\n%f %f %f\n%f %f %f\n%f %f %f\n\n",
                            transformMat.v[0],
                            transformMat.v[1],
                            transformMat.v[2],
                            transformMat.v[3],
                            transformMat.v[4],
                            transformMat.v[5],
                            transformMat.v[6],
                            transformMat.v[7],
                            transformMat.v[8],
                            transformMat.v[9],
                            transformMat.v[10],
                            transformMat.v[11]
                        );
                        #endif

                        // Transpose and scale
                        // why scale? I guess meters to cm?
                        ufbx_matrix tempMat;
                        for (int i = 0; i < 4; ++i) {
                            for (int j = 0; j < 3; ++j) {
                                int idx_in = i * 3 + j;
                                int idx_out = j * 4 + i;
                                float scale = 1.0;
                                /*if (i != 3) {
                                    scale = 0.0001;
                                }*/
                                /*if(i == 3) {
                                    scale = 0.01;
                                }*/
                                tempMat.v[idx_out] = transformMat.v[idx_in] * scale;
                            }
                        }

                        // Do some funny coordinate swaps to make the homogenous part end up at the left side of the matrix
                        // because that's what the C3D matrices want.
                        ufbx_matrix tempMat2;
                        tempMat2.v[ 0] = tempMat.v[ 3];
                        tempMat2.v[ 1] = tempMat.v[ 0];
                        tempMat2.v[ 2] = tempMat.v[ 1];
                        tempMat2.v[ 3] = tempMat.v[ 2];

                        tempMat2.v[ 4] = tempMat.v[ 7];
                        tempMat2.v[ 5] = tempMat.v[ 4];
                        tempMat2.v[ 6] = tempMat.v[ 5];
                        tempMat2.v[ 7] = tempMat.v[ 6];

                        tempMat2.v[ 8] = tempMat.v[11];
                        tempMat2.v[ 9] = tempMat.v[ 8];
                        tempMat2.v[10] = tempMat.v[ 9];
                        tempMat2.v[11] = tempMat.v[10];

                        // Now swap second and third column as well as the first and third row
                        // because it is illegal for modeling tools and engines to ever use the
                        // same coordinate system.
                        tempMat.v[ 0] = tempMat2.v[ 4];
                        tempMat.v[ 1] = tempMat2.v[ 7];
                        tempMat.v[ 2] = tempMat2.v[ 6];
                        tempMat.v[ 3] = tempMat2.v[ 5];

                        tempMat.v[ 4] = tempMat2.v[ 8];
                        tempMat.v[ 5] = tempMat2.v[11];
                        tempMat.v[ 6] = tempMat2.v[10];
                        tempMat.v[ 7] = tempMat2.v[ 9];

                        tempMat.v[ 8] = tempMat2.v[ 0];
                        tempMat.v[ 9] = tempMat2.v[ 3];
                        tempMat.v[10] = tempMat2.v[ 2];
                        tempMat.v[11] = tempMat2.v[ 1];

                        // Finally, we have done it and other than a rotation by 90 degrees around z (on device y. the one pointing up.)
                        // we now have things pointing the same direction as in the modeling tools, assuming a camera at
                        // negative y and looking at the origin.
                        for(int i = 0; i < 12; i++) {
                            objectNew.animFrames[FBX_FRAME_IDX(frame, boneIdx, i, boneCount)] = tempMat.v[i];
                        }

                        #ifdef DEBUG
                        printf("%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n\n",
                            objectNew.animFrames[FBX_FRAME_IDX(frame, boneIdx, 0, boneCount)],
                            objectNew.animFrames[FBX_FRAME_IDX(frame, boneIdx, 1, boneCount)],
                            objectNew.animFrames[FBX_FRAME_IDX(frame, boneIdx, 2, boneCount)],
                            objectNew.animFrames[FBX_FRAME_IDX(frame, boneIdx, 3, boneCount)],
                            objectNew.animFrames[FBX_FRAME_IDX(frame, boneIdx, 4, boneCount)],
                            objectNew.animFrames[FBX_FRAME_IDX(frame, boneIdx, 5, boneCount)],
                            objectNew.animFrames[FBX_FRAME_IDX(frame, boneIdx, 6, boneCount)],
                            objectNew.animFrames[FBX_FRAME_IDX(frame, boneIdx, 7, boneCount)],
                            objectNew.animFrames[FBX_FRAME_IDX(frame, boneIdx, 8, boneCount)],
                            objectNew.animFrames[FBX_FRAME_IDX(frame, boneIdx, 9, boneCount)],
                            objectNew.animFrames[FBX_FRAME_IDX(frame, boneIdx, 10, boneCount)],
                            objectNew.animFrames[FBX_FRAME_IDX(frame, boneIdx, 11, boneCount)]
                        );
                        #endif                        
                    }
                }
            }
            else {
                printf("Yikes! Invalid animation stack count (= %ld) or no skin present!\n", scene->anim_stacks.count);
            }
        }
        else {
            printf("Yikes! Object is meshless! This shouldn't happen!\n");
        }
    }
    
    return objectNew;
}

void serializeObject(fbxBasedObject* object, const char* filename) {
    FILE* fp = fopen(filename, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Error: failed to open file for writing.\n");
        exit(1);
    }

    // Write the vertCount and frameCount to the file
    fwrite(&object->vertCount, sizeof(int32_t), 1, fp);
    fwrite(&object->boneCount, sizeof(int32_t), 1, fp);
    fwrite(&object->frameCount, sizeof(int32_t), 1, fp);

    // Write the vbo to the file
    fwrite(object->vbo, sizeof(vertex_rigged), object->vertCount, fp);

    // Write the animFrames to the file
    fwrite(object->animFrames, sizeof(float), object->frameCount * object->boneCount * 12, fp);

    fclose(fp);
}

int getMeshyObjects(const char* filename, char*** objectNames) {
    // Load data
    ufbx_load_opts opts = { 0 }; // Optional, pass NULL for defaults
    ufbx_error error; // Optional, pass NULL if you don't care about errors
    ufbx_scene *scene = ufbx_load_file(filename, &opts, &error);
    if (!scene) {
        fprintf(stderr, "Failed to load: %s\n", error.description.data);
    }

    // Alloc space for object name char arrays
    *objectNames = malloc(scene->nodes.count * sizeof(char*));

    // Go through all objects in the scene
    int foundObjects = 0;
    for (size_t i = 0; i < scene->nodes.count; i++) {
        ufbx_node *node = scene->nodes.data[i];
        if (node->is_root) continue;

        // We can only load meshes
        if (node->mesh) {
            // Get the name of the object and copy it over
            char* objectName = malloc(strlen(node->name.data) + 1);
            strcpy(objectName, node->name.data);
            (*objectNames)[foundObjects] = objectName;
            foundObjects += 1;
        }
    }
    return foundObjects;
}

int main(int argc, char** argv) {
    if(argc != 3) {
        printf("usage: ufbxconv <infile.fbx> <base name or directory>\n");
        exit(1);
    }
    const char* infile = argv[1];
    const char* basename = argv[2];

    // infile name without dir name or extension for output name building
    char* name = malloc(strlen(infile) + 1);
    strcpy(name, infile);
    char* lastSlash = strrchr(name, '/');
    if(lastSlash != NULL) {
        name = lastSlash + 1;
    }
    char* lastDot = strrchr(name, '.');
    if(lastDot != NULL) {
        *lastDot = '\0';
    }
    printf("Processing %s.fbx...\n", name);

    // Find all the objects in the file
    char** objectNames;
    int nbFoundObjects = getMeshyObjects(infile, &objectNames);
    printf("Found %d objects.\n", nbFoundObjects);
    for (int i = 0; i < nbFoundObjects; i++) {
        // Out name is basename + file name + _  + object name + .vbo
        char* outName = malloc(strlen(basename) + strlen(name) + strlen(objectNames[i]) + 6);
        strcpy(outName, basename);
        strcat(outName, name);
        strcat(outName, "_");
        strcat(outName, objectNames[i]);
        strcat(outName, ".vbo");
        printf("* Converting: %s to %s.\n", objectNames[i], outName);
        fbxBasedObject object = loadFBXObject(infile, objectNames[i]);
        printf("   -> Have %d vertices, %d anim frames.\n", object.vertCount, object.frameCount);
        serializeObject(&object, outName);
    }
    
    // Quitting the program automatically frees all the memory, how convenient
}
