#ifndef SKINNEDMODEL_H
#define SKINNEDMODEL_H

/*
* Arena Limpia - modelo con esqueleto y animaciones (skinning en el vertex shader).
*
* Basado en AnimatedModel de la práctica 09, con estos cambios:
*  - Los huesos se numeran por nombre para todo el modelo (no por malla), así un modelo con
*    varias mallas usa un solo arreglo gBones.
*  - Si el archivo no trae normales, Assimp las calcula (los FBX de Blender 5 llegan sin ellas).
*  - Los nodos sin animación conservan su transformación (el original usaba la identidad).
*  - Se elige la animación por nombre ("Caminar", "Parado") y el tiempo va en segundos.
*  - Usa el color del material si la malla no tiene textura, igual que Model.
*/

#include <modelstructs.h>
#include <cfloat>
#include <glm/gtc/type_ptr.hpp>

#define MAX_SKIN_BONES 100

class SkinnedModel
{
public:
    vector<Mesh>    meshes;
    vector<Texture> textures_loaded;
    string          directory;

    Assimp::Importer importer;
    const aiScene*   scene = nullptr;

    map<string, int>  boneIndex;     // nombre del hueso -> índice en gBones
    vector<glm::mat4> boneOffset;    // de espacio de malla a espacio del hueso (pose de reposo)
    glm::mat4         gBones[MAX_SKIN_BONES];
    map<string, glm::mat4> boneGlobal; // transformación de cada hueso en la pose actual (espacio del modelo)
    glm::mat4         globalInverse = glm::mat4(1.0f);

    // Caja envolvente en la pose del primer cuadro de la primera animación
    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(0.0f);

    explicit SkinnedModel(const string& path)
    {
        for (glm::mat4& m : gBones) m = glm::mat4(1.0f);
        load(path);
    }

    bool valid() const { return scene != nullptr && !meshes.empty(); }

    // Índice de una animación por nombre, o -1 si no existe
    int findAnimation(const string& name) const
    {
        if (!scene) return -1;
        for (unsigned int i = 0; i < scene->mNumAnimations; i++)
            if (name == scene->mAnimations[i]->mName.C_Str())
                return (int)i;
        return -1;
    }

    // Duración de una animación en segundos
    float duration(int anim) const
    {
        if (!scene || anim < 0 || anim >= (int)scene->mNumAnimations) return 0.0f;
        const aiAnimation* a = scene->mAnimations[anim];
        double tps = a->mTicksPerSecond > 0.0 ? a->mTicksPerSecond : 24.0;
        return (float)(a->mDuration / tps);
    }

    // Calcula gBones para la animación anim en el instante t (segundos, se repite en ciclo)
    void setPose(int anim, float t)
    {
        if (!scene || anim < 0 || anim >= (int)scene->mNumAnimations) return;
        const aiAnimation* a = scene->mAnimations[anim];
        double tps = a->mTicksPerSecond > 0.0 ? a->mTicksPerSecond : 24.0;
        double ticks = a->mDuration > 0.0 ? fmod((double)t * tps, a->mDuration) : 0.0;
        if (ticks < 0.0) ticks += a->mDuration;
        readNodeHierarchy(a, (float)ticks, scene->mRootNode, glm::mat4(1.0f));
    }

    // Posición (espacio del modelo) del origen de un hueso en la pose actual: su articulación.
    // Sirve para saber dónde está la mano y poner ahí el balón.
    glm::vec3 bonePosition(const string& name) const
    {
        auto it = boneGlobal.find(name);
        if (it == boneGlobal.end()) return glm::vec3(0.0f);
        return glm::vec3(it->second[3]);
    }

    void Draw(Shader& shader)
    {
        shader.setMat4("gBones", MAX_SKIN_BONES, gBones);
        for (Mesh& mesh : meshes)
            mesh.Draw(shader);
    }

private:
    static glm::mat4 toGlm(const aiMatrix4x4& m)
    {
        return glm::transpose(glm::make_mat4(&m.a1));
    }

    void load(const string& path)
    {
        scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs |
                                        aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
                                        aiProcess_LimitBoneWeights);
        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
            cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
            scene = nullptr;
            return;
        }
        directory = path.substr(0, path.find_last_of('/'));
        globalInverse = glm::inverse(toGlm(scene->mRootNode->mTransformation));

        processNode(scene->mRootNode);

        cout << "  " << boneIndex.size() << " huesos, " << scene->mNumAnimations << " animaciones:";
        for (unsigned int i = 0; i < scene->mNumAnimations; i++)
            cout << " " << scene->mAnimations[i]->mName.C_Str() << " (" << duration(i) << " s)";
        cout << endl;

        setPose(0, 0.0f);
        computeBounds();
    }

    void processNode(aiNode* node)
    {
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
            meshes.push_back(processMesh(scene->mMeshes[node->mMeshes[i]]));
        for (unsigned int i = 0; i < node->mNumChildren; i++)
            processNode(node->mChildren[i]);
    }

    Mesh processMesh(aiMesh* mesh)
    {
        vector<Vertex> vertices(mesh->mNumVertices);
        vector<int> boneCount(mesh->mNumVertices, 0);

        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex v{};
            v.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
            if (mesh->HasNormals())
                v.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            if (mesh->mTextureCoords[0])
                v.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
            if (mesh->HasTangentsAndBitangents()) {
                v.Tangent   = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
                v.Bitangent = glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
            }
            vertices[i] = v;
        }

        // Pesos: hasta 4 huesos por vértice (aiProcess_LimitBoneWeights), en IDs1/Weights1
        for (unsigned int b = 0; b < mesh->mNumBones; b++) {
            const aiBone* bone = mesh->mBones[b];
            string name = bone->mName.C_Str();
            int index;
            auto it = boneIndex.find(name);
            if (it == boneIndex.end()) {
                index = (int)boneOffset.size();
                if (index >= MAX_SKIN_BONES) continue;
                boneIndex[name] = index;
                boneOffset.push_back(toGlm(bone->mOffsetMatrix));
            } else {
                index = it->second;
            }
            for (unsigned int w = 0; w < bone->mNumWeights; w++) {
                unsigned int vid = bone->mWeights[w].mVertexId;
                int& n = boneCount[vid];
                if (n < 4) {
                    vertices[vid].IDs1[n] = (float)index;
                    vertices[vid].Weights1[n] = bone->mWeights[w].mWeight;
                    n++;
                }
            }
        }

        vector<unsigned int> indices;
        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
            for (unsigned int j = 0; j < mesh->mFaces[i].mNumIndices; j++)
                indices.push_back(mesh->mFaces[i].mIndices[j]);

        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        vector<Texture> textures = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");

        aiColor4D kd(0.8f, 0.8f, 0.8f, 1.0f);
        aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &kd);
        return Mesh(vertices, indices, textures, glm::vec4(kd.r, kd.g, kd.b, 1.0f));
    }

    vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, const string& typeName)
    {
        vector<Texture> textures;
        for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
            aiString str;
            mat->GetTexture(type, i, &str);
            bool skip = false;
            for (const Texture& t : textures_loaded) {
                if (t.path == str.C_Str()) {
                    textures.push_back(t);
                    skip = true;
                    break;
                }
            }
            if (!skip) {
                Texture texture;
                texture.id = TextureFromFile(str.C_Str(), directory);
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                textures_loaded.push_back(texture);
            }
        }
        return textures;
    }

    // Caja envolvente aplicando la pose actual a los vértices (en la CPU, solo al cargar)
    void computeBounds()
    {
        glm::vec3 mn(FLT_MAX), mx(-FLT_MAX);
        for (const Mesh& mesh : meshes) {
            for (const Vertex& v : mesh.vertices) {
                glm::mat4 skin(0.0f);
                float total = 0.0f;
                for (int k = 0; k < 4; k++) {
                    skin += gBones[(int)v.IDs1[k]] * v.Weights1[k];
                    total += v.Weights1[k];
                }
                if (total < 0.001f) skin = glm::mat4(1.0f);
                glm::vec3 p = glm::vec3(skin * glm::vec4(v.Position, 1.0f));
                mn = glm::min(mn, p);
                mx = glm::max(mx, p);
            }
        }
        if (mn.x > mx.x) mn = mx = glm::vec3(0.0f);
        boundsMin = mn;
        boundsMax = mx;
    }

    void readNodeHierarchy(const aiAnimation* anim, float ticks, const aiNode* node, const glm::mat4& parent)
    {
        string name = node->mName.C_Str();
        glm::mat4 local = toGlm(node->mTransformation);

        const aiNodeAnim* channel = findChannel(anim, name);
        if (channel) {
            aiVector3D s = interpolate(channel->mScalingKeys, channel->mNumScalingKeys, ticks);
            aiVector3D p = interpolate(channel->mPositionKeys, channel->mNumPositionKeys, ticks);
            aiQuaternion r = interpolateRotation(channel, ticks);
            glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(p.x, p.y, p.z));
            glm::mat4 R = toGlm(aiMatrix4x4(r.GetMatrix()));
            glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(s.x, s.y, s.z));
            local = T * R * S;
        }

        glm::mat4 global = parent * local;
        auto it = boneIndex.find(name);
        if (it != boneIndex.end()) {
            gBones[it->second] = globalInverse * global * boneOffset[it->second];
            boneGlobal[name] = globalInverse * global;
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++)
            readNodeHierarchy(anim, ticks, node->mChildren[i], global);
    }

    static const aiNodeAnim* findChannel(const aiAnimation* anim, const string& name)
    {
        for (unsigned int i = 0; i < anim->mNumChannels; i++)
            if (name == anim->mChannels[i]->mNodeName.C_Str())
                return anim->mChannels[i];
        return nullptr;
    }

    static aiVector3D interpolate(const aiVectorKey* keys, unsigned int count, float ticks)
    {
        if (count == 0) return aiVector3D(1.0f, 1.0f, 1.0f);
        if (count == 1 || ticks <= (float)keys[0].mTime) return keys[0].mValue;
        for (unsigned int i = 0; i + 1 < count; i++) {
            if (ticks < (float)keys[i + 1].mTime) {
                float dt = (float)(keys[i + 1].mTime - keys[i].mTime);
                float f = dt > 0.0f ? (ticks - (float)keys[i].mTime) / dt : 0.0f;
                return keys[i].mValue + (keys[i + 1].mValue - keys[i].mValue) * f;
            }
        }
        return keys[count - 1].mValue;
    }

    static aiQuaternion interpolateRotation(const aiNodeAnim* ch, float ticks)
    {
        const aiQuatKey* keys = ch->mRotationKeys;
        unsigned int count = ch->mNumRotationKeys;
        if (count == 0) return aiQuaternion();
        if (count == 1 || ticks <= (float)keys[0].mTime) return keys[0].mValue;
        for (unsigned int i = 0; i + 1 < count; i++) {
            if (ticks < (float)keys[i + 1].mTime) {
                float dt = (float)(keys[i + 1].mTime - keys[i].mTime);
                float f = dt > 0.0f ? (ticks - (float)keys[i].mTime) / dt : 0.0f;
                aiQuaternion out;
                aiQuaternion::Interpolate(out, keys[i].mValue, keys[i + 1].mValue, f);
                return out.Normalize();
            }
        }
        return keys[count - 1].mValue;
    }
};

#endif
