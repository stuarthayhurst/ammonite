#include <iostream>
#include <vector>
#include <map>
#include <cstring>
#include <string>

#include <GL/glew.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "internal/textures.hpp"
#include "internal/modelTracker.hpp"
#include "internal/lightTracker.hpp"
#include "utils/logging.hpp"

#include "internal/internalDebug.hpp"

namespace ammonite {
  namespace {
    //Track all loaded models
    std::map<int, models::ModelInfo> modelTrackerMap;
    std::map<std::string, models::ModelData> modelDataMap;

    struct ModelLoadInfo {
      std::string modelDirectory;
      bool flipTexCoords;
      bool srgbTextures;
    };

    //Constants for loading assumptions
    const bool ASSUME_FLIP_UVS = true;
    const bool ASSUME_SRGB_TEXTURES = false;
  }

  //Internally exposed model handling methods
  namespace models {
    ModelInfo* getModelPtr(int modelId) {
      //Check the model exists, and return a pointer
      auto it = modelTrackerMap.find(modelId);
      if (it != modelTrackerMap.end()) {
        return &it->second;
      } else {
        return nullptr;
      }
    }

    void setLightEmitting(int modelId, bool lightEmitting) {
      ModelInfo* modelPtr = models::getModelPtr(modelId);
      if (modelPtr != nullptr) {
        modelPtr->isLightEmitting = lightEmitting;
      }
    }

    bool getLightEmitting(int modelId) {
      ModelInfo* modelPtr = models::getModelPtr(modelId);
      if (modelPtr != nullptr) {
        return modelPtr->isLightEmitting;
      }
      return false;
    }
  }

  namespace {
    static void createBuffers(models::ModelData* modelObjectData) {
      //Generate buffers for every mesh
      for (unsigned int i = 0; i < modelObjectData->meshes.size(); i++) {
        models::MeshData* meshData = &modelObjectData->meshes[i];

        //Create vertex and index buffers
        glCreateBuffers(2, &meshData->vertexBufferId);

        //Fill interleaved vertex + normal + texture buffer and index buffer
        glNamedBufferData(meshData->vertexBufferId, meshData->meshData.size() * sizeof(models::VertexData), &meshData->meshData[0], GL_STATIC_DRAW);
        glNamedBufferData(meshData->elementBufferId, meshData->indices.size() * sizeof(unsigned int), &meshData->indices[0], GL_STATIC_DRAW);

        //Create the vertex attribute buffer
        glCreateVertexArrays(1, &meshData->vertexArrayId);

        GLuint vaoId = meshData->vertexArrayId;
        GLuint vboId = meshData->vertexBufferId;
        int stride = 8 * sizeof(float); //(3 + 3 + 2) * bytes per float

        //Vertex attribute
        glEnableVertexArrayAttrib(vaoId, 0);
        glVertexArrayVertexBuffer(vaoId, 0, vboId, 0, stride);
        glVertexArrayAttribFormat(vaoId, 0, 3, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(vaoId, 0, 0);

        //Normal attribute
        glEnableVertexArrayAttrib(vaoId, 1);
        glVertexArrayVertexBuffer(vaoId, 1, vboId, 3 * sizeof(float), stride);
        glVertexArrayAttribFormat(vaoId, 1, 3, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(vaoId, 1, 1);

        //Texture attribute
        glEnableVertexArrayAttrib(vaoId, 2);
        glVertexArrayVertexBuffer(vaoId, 2, vboId, 6 * sizeof(float), stride);
        glVertexArrayAttribFormat(vaoId, 2, 2, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(vaoId, 2, 2);

        //Element buffer
        glVertexArrayElementBuffer(vaoId, meshData->elementBufferId);
      }
    }

    static void deleteBuffers(models::ModelData* modelObjectData) {
      //Delete created buffers and the VAO
      for (unsigned int i = 0; i < modelObjectData->meshes.size(); i++) {
        models::MeshData* meshData = &modelObjectData->meshes[i];

        glDeleteBuffers(1, &meshData->vertexBufferId);
        glDeleteBuffers(1, &meshData->elementBufferId);
        glDeleteVertexArrays(1, &meshData->vertexArrayId);
      }
    }

    static void processMesh(aiMesh* mesh, const aiScene* scene, std::vector<models::MeshData>* meshes, std::vector<GLuint>* textureIds, ModelLoadInfo modelLoadInfo, bool* externalSuccess) {
      //Add a new empty mesh to the mesh vector
      meshes->emplace_back();
      models::MeshData* newMesh = &meshes->back();

      //Fill the mesh with vertex data
      for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        models::VertexData vertexData;

        vertexData.vertex.x = mesh->mVertices[i].x;
        vertexData.vertex.y = mesh->mVertices[i].y;
        vertexData.vertex.z = mesh->mVertices[i].z;

        vertexData.normal.x = mesh->mNormals[i].x;
        vertexData.normal.y = mesh->mNormals[i].y;
        vertexData.normal.z = mesh->mNormals[i].z;

        if (mesh->mTextureCoords[0]) {
          vertexData.texturePoint.x = mesh->mTextureCoords[0][i].x;
          vertexData.texturePoint.y = mesh->mTextureCoords[0][i].y;
        } else {
          vertexData.texturePoint = glm::vec2(0.0f);
        }

        newMesh->meshData.push_back(vertexData);
      }

      //Fill mesh indices
      for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
          newMesh->indices.push_back(face.mIndices[j]);
        }
      }
      newMesh->vertexCount = newMesh->indices.size();

      //Load any diffuse texture given
      aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
      if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
        aiString texturePath;
        material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath);

        std::string fullTexturePath = modelLoadInfo.modelDirectory + '/' + texturePath.C_Str();

        bool hasCreatedTexture = true;
        int textureId = ammonite::textures::loadTexture(fullTexturePath.c_str(), modelLoadInfo.srgbTextures, &hasCreatedTexture);
        if (!hasCreatedTexture) {
          *externalSuccess = false;
          return;
        }

        textureIds->push_back(textureId);
      } else {
        //Add an empty texture to the tracker, to keep pace with meshes
        textureIds->push_back(0);
      }
    }

    static void processNode(aiNode* node, const aiScene* scene, std::vector<models::MeshData>* meshes, std::vector<GLuint>* textureIds, ModelLoadInfo modelLoadInfo, bool* externalSuccess) {
      for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        processMesh(scene->mMeshes[node->mMeshes[i]], scene, meshes, textureIds, modelLoadInfo, externalSuccess);
      }

      for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene, meshes, textureIds, modelLoadInfo, externalSuccess);
      }
    }

    static void loadObject(const char* objectPath, models::ModelData* modelObjectData, std::vector<GLuint>* textureIds, ModelLoadInfo modelLoadInfo, bool* externalSuccess) {
      //Generate postprocessing flags
      auto aiProcessFlags = aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_GenUVCoords | aiProcess_RemoveRedundantMaterials | aiProcess_OptimizeMeshes | aiProcess_JoinIdenticalVertices | aiProcess_PreTransformVertices;

      //Flip texture coords, if requested
      if (modelLoadInfo.flipTexCoords) {
        aiProcessFlags = aiProcessFlags | aiProcess_FlipUVs;
      }

      Assimp::Importer importer;
      const aiScene *scene = importer.ReadFile(objectPath, aiProcessFlags);

      //Check model loaded correctly
      if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << ammonite::utils::warning << importer.GetErrorString() << std::endl;
        *externalSuccess = false;
        return;
      }

      //Recursively process nodes
      processNode(scene->mRootNode, scene, &modelObjectData->meshes, textureIds, modelLoadInfo, externalSuccess);
    }
  }

  //Exposed model handling methods
  namespace models {
    namespace {
      static void calcModelMatrices(models::PositionData* positionData) {
        //Recalculate the model matrix when a component changes
        positionData->modelMatrix = positionData->translationMatrix * glm::toMat4(positionData->rotationQuat) * positionData->scaleMatrix;

        //Normal matrix
        positionData->normalMatrix = glm::transpose(glm::inverse(positionData->modelMatrix));
      }

      //Track cumulative number of created models
      int totalModels = 0;
    }

    int createModel(const char* objectPath, bool flipTexCoords, bool srgbTextures, bool* externalSuccess) {
      //Create the model
      ModelInfo modelObject;
      modelObject.modelName = std::string(objectPath);

      //Reuse model data if it has already been loaded
      auto it = modelDataMap.find(modelObject.modelName);
      if (it != modelDataMap.end()) {
        modelObject.modelData = &it->second;
        modelObject.modelData->refCount++;
      } else {
        //Create empty ModelData object and add to tracker
        ModelData newModelData;
        modelDataMap[modelObject.modelName] = newModelData;
        modelObject.modelData = &modelDataMap[modelObject.modelName];

        //Generate info required to load model
        std::string pathString = objectPath;
        ModelLoadInfo modelLoadInfo;
        modelLoadInfo.flipTexCoords = flipTexCoords;
        modelLoadInfo.srgbTextures = srgbTextures;
        modelLoadInfo.modelDirectory = pathString.substr(0, pathString.find_last_of('/'));

        //Fill the model data
        bool hasCreatedObject = true;
        loadObject(objectPath, modelObject.modelData, &modelObject.textureIds, modelLoadInfo, &hasCreatedObject);
        if (!hasCreatedObject) {
          modelDataMap.erase(modelObject.modelName);
          *externalSuccess = false;
          return 0;
        }
        createBuffers(modelObject.modelData);
      }

      PositionData positionData;
      positionData.translationMatrix = glm::mat4(1.0f);
      positionData.scaleMatrix = glm::mat4(1.0f);
      positionData.rotationQuat = glm::quat(glm::vec3(0, 0, 0));

      modelObject.positionData = positionData;

      //Calculate model and normal matrices
      calcModelMatrices(&modelObject.positionData);

      //Add model to the tracker and return the ID
      modelObject.modelId = ++totalModels;
      modelTrackerMap[modelObject.modelId] = modelObject;
      return modelObject.modelId;
    }

    int createModel(const char* objectPath, bool* externalSuccess) {
      return createModel(objectPath, ASSUME_FLIP_UVS, ASSUME_SRGB_TEXTURES, externalSuccess);
    }

    int copyModel(int modelId) {
      //Get the model and check it exists
      models::ModelInfo* oldModelObject = models::getModelPtr(modelId);
      if (oldModelObject == nullptr) {
        return 0;
      }

      //Copy model data
      ModelInfo modelObject = *oldModelObject;
      modelObject.isLightEmitting = false;
      modelObject.modelData->refCount++;

      //Add model to the tracker and return the ID
      modelObject.modelId = ++totalModels;
      modelTrackerMap[modelObject.modelId] = modelObject;
      return modelObject.modelId;
    }

    void unloadModel(int modelId) {
      ModelInfo* modelPtr = models::getModelPtr(modelId);
      if (modelPtr == nullptr) {
        return;
      }

      //Check if model is loaded
      if (modelPtr->isLoaded) {
        //Set model as unloaded
        modelPtr->isLoaded = false;

        //Decrease refcount, add a new soft reference
        modelPtr->modelData->softRefCount++;
        modelPtr->modelData->refCount--;

        //Unload actual data if no models reference it
        if (modelPtr->modelData->refCount < 1) {
          deleteBuffers(modelPtr->modelData);
        }
      }
    }

    void reloadModel(int modelId) {
      ModelInfo* modelPtr = models::getModelPtr(modelId);
      if (modelPtr == nullptr) {
        return;
      }

      //Check if model is unloaded
      if (!modelPtr->isLoaded) {
        //Set model as loaded
        modelPtr->isLoaded = true;

        //Increase refcount, remove a soft reference
        modelPtr->modelData->softRefCount--;
        modelPtr->modelData->refCount++;

        //Upload actual data to the GPU, if it wasn't present
        if (modelPtr->modelData->refCount == 1) {
          createBuffers(modelPtr->modelData);
        }
      }
    }

    void deleteModel(int modelId) {
      //Check the model actually exists
      auto it = modelTrackerMap.find(modelId);
      if (it != modelTrackerMap.end()) {
        ModelInfo* modelObject = &it->second;
        ModelData* modelObjectData = modelObject->modelData;
        //Decrease the reference / soft reference count of the model data
        if (modelObject->isLoaded) {
          modelObjectData->refCount--;
        } else {
          modelObjectData->softRefCount--;
        }

        //If the model data is now unused, destroy it
        if (modelObjectData->refCount < 1 and modelObjectData->softRefCount < 1) {
          //Reduce reference count on textures
          for (unsigned int i = 0; i < modelObjectData->meshes.size(); i++) {
            ammonite::textures::deleteTexture(modelObject->textureIds[i]);
          }

          //Destroy the model buffers and position in second tracker layer
          deleteBuffers(modelObjectData);
          modelDataMap.erase(modelObject->modelName);
        }

        //Unlink any attached light source
        ammonite::lighting::unlinkByModel(modelId);

        //Remove the model from the tracker
        modelTrackerMap.erase(modelId);
      }
    }

    void applyTexture(int modelId, const char* texturePath, bool srgbTexture, bool* externalSuccess) {
      ModelInfo* modelPtr = models::getModelPtr(modelId);
      if (modelPtr == nullptr) {
        *externalSuccess = false;
        return;
      }

      //Apply texture to every mesh on the model
      ModelData* modelObjectData = modelPtr->modelData;
      for (unsigned int i = 0; i < modelObjectData->meshes.size(); i++) {
        //If a texture is already applied, remove it
        if (modelPtr->textureIds[i] != 0) {
          ammonite::textures::deleteTexture(modelPtr->textureIds[i]);
          modelPtr->textureIds[i] = 0;
        }

        //Create new texture and apply to the mesh
        bool hasCreatedTexture = true;
        int textureId = ammonite::textures::loadTexture(texturePath, srgbTexture, &hasCreatedTexture);
        if (!hasCreatedTexture) {
          *externalSuccess = false;
          return;
        }

        modelPtr->textureIds[i] = textureId;
      }
    }

    void applyTexture(int modelId, const char* texturePath, bool* externalSuccess) {
      applyTexture(modelId, texturePath, ASSUME_SRGB_TEXTURES, externalSuccess);
    }

    //Return the number of vertices on a model
    int getVertexCount(int modelId) {
      ModelInfo* modelPtr = models::getModelPtr(modelId);
      if (modelPtr == nullptr) {
        return 0;
      }

      int vertexCount = 0;
      models::ModelData* modelData = modelPtr->modelData;
      for (unsigned int i = 0; i < modelData->meshes.size(); i++) {
        vertexCount += modelData->meshes[i].vertexCount;
      }

      return vertexCount;
    }

    namespace draw {
      void setDrawMode(int modelId, int drawMode) {
        ModelInfo* modelPtr = models::getModelPtr(modelId);
        if (modelPtr != nullptr) {
          modelPtr->drawMode = drawMode;
        }
      }

      void setActive(int modelId, bool active) {
        ModelInfo* modelPtr = models::getModelPtr(modelId);
        if (modelPtr != nullptr) {
          modelPtr->isActive = active;
        }
      }
    }

    //Return position, scale and rotation of a model
    namespace position {
      glm::vec3 getPosition(int modelId) {
        //Get the model and check it exists
        models::ModelInfo* modelObject = models::getModelPtr(modelId);
        if (modelObject == nullptr) {
          return glm::vec3(0.0f);
        }

        return glm::vec3(modelObject->positionData.translationMatrix * glm::vec4(glm::vec3(0, 0, 0), 1));
      }

      glm::vec3 getScale(int modelId) {
        //Get the model and check it exists
        models::ModelInfo* modelObject = models::getModelPtr(modelId);
        if (modelObject == nullptr) {
          return glm::vec3(0.0f);
        }

        return glm::vec3(modelObject->positionData.scaleMatrix * glm::vec4(glm::vec3(1, 1, 1), 1));
      }

      glm::vec3 getRotation(int modelId) {
        //Get the model and check it exists
        models::ModelInfo* modelObject = models::getModelPtr(modelId);
        if (modelObject == nullptr) {
          return glm::vec3(0.0f);
        }

        return glm::degrees(glm::eulerAngles(modelObject->positionData.rotationQuat));
      }
    }

    //Set absolute position, scale and rotation of models
    namespace position {
      void setPosition(int modelId, glm::vec3 position) {
        //Get the model and check it exists
        models::ModelInfo* modelObject = models::getModelPtr(modelId);
        if (modelObject == nullptr) {
          return;
        }

        //Set the position
        modelObject->positionData.translationMatrix = glm::translate(glm::mat4(1.0f), position);

        //Recalculate model and normal matrices
        calcModelMatrices(&modelObject->positionData);
      }

      void setScale(int modelId, glm::vec3 scale) {
        //Get the model and check it exists
        models::ModelInfo* modelObject = models::getModelPtr(modelId);
        if (modelObject == nullptr) {
          return;
        }

        //Set the scale
        modelObject->positionData.scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

        //Recalculate model and normal matrices
        calcModelMatrices(&modelObject->positionData);
      }

      void setScale(int modelId, float scaleMultiplier) {
        setScale(modelId, glm::vec3(scaleMultiplier, scaleMultiplier, scaleMultiplier));
      }

      void setRotation(int modelId, glm::vec3 rotation) {
        //Get the model and check it exists
        models::ModelInfo* modelObject = models::getModelPtr(modelId);
        if (modelObject == nullptr) {
          return;
        }

        //Set the rotation
        glm::vec3 rotationRadians = glm::vec3(glm::radians(rotation[0]),
                                              glm::radians(rotation[1]),
                                              glm::radians(rotation[2]));
        modelObject->positionData.rotationQuat = glm::quat(rotationRadians) * glm::quat(glm::vec3(0, 0, 0));

        //Recalculate model and normal matrices
        calcModelMatrices(&modelObject->positionData);
      }
    }

    //Translate, scale and rotate models
    namespace position {
      void translateModel(int modelId, glm::vec3 translation) {
        //Get the model and check it exists
        models::ModelInfo* modelObject = models::getModelPtr(modelId);
        if (modelObject == nullptr) {
          return;
        }

        //Translate it
        modelObject->positionData.translationMatrix = glm::translate(
          modelObject->positionData.translationMatrix,
          translation);

        //Recalculate model and normal matrices
        calcModelMatrices(&modelObject->positionData);
      }

      void scaleModel(int modelId, glm::vec3 scaleVector) {
        //Get the model and check it exists
        models::ModelInfo* modelObject = models::getModelPtr(modelId);
        if (modelObject == nullptr) {
          return;
        }

        //Scale it
        modelObject->positionData.scaleMatrix = glm::scale(
          modelObject->positionData.scaleMatrix,
          scaleVector);

        //Recalculate model and normal matrices
        calcModelMatrices(&modelObject->positionData);
      }

      void scaleModel(int modelId, float scaleMultiplier) {
        scaleModel(modelId, glm::vec3(scaleMultiplier, scaleMultiplier, scaleMultiplier));
      }

      void rotateModel(int modelId, glm::vec3 rotation) {
        //Get the model and check it exists
        models::ModelInfo* modelObject = models::getModelPtr(modelId);
        if (modelObject == nullptr) {
          return;
        }

        //Rotate it
        glm::vec3 rotationRadians = glm::vec3(glm::radians(rotation[0]),
                                              glm::radians(rotation[1]),
                                              glm::radians(rotation[2]));
        modelObject->positionData.rotationQuat = glm::quat(rotationRadians) * modelObject->positionData.rotationQuat;

        //Recalculate model and normal matrices
        calcModelMatrices(&modelObject->positionData);
      }
    }
  }
}
