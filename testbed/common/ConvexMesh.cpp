/********************************************************************************
* ReactPhysics3D physics library, http://www.reactphysics3d.com                 *
* Copyright (c) 2010-2016 Daniel Chappuis                                       *
*********************************************************************************
*                                                                               *
* This software is provided 'as-is', without any express or implied warranty.   *
* In no event will the authors be held liable for any damages arising from the  *
* use of this software.                                                         *
*                                                                               *
* Permission is granted to anyone to use this software for any purpose,         *
* including commercial applications, and to alter it and redistribute it        *
* freely, subject to the following restrictions:                                *
*                                                                               *
* 1. The origin of this software must not be misrepresented; you must not claim *
*    that you wrote the original software. If you use this software in a        *
*    product, an acknowledgment in the product documentation would be           *
*    appreciated but is not required.                                           *
*                                                                               *
* 2. Altered source versions must be plainly marked as such, and must not be    *
*    misrepresented as being the original software.                             *
*                                                                               *
* 3. This notice may not be removed or altered from any source distribution.    *
*                                                                               *
********************************************************************************/

// Libraries
#include "ConvexMesh.h"

// Constructor
ConvexMesh::ConvexMesh(const openglframework::Vector3 &position,
                       reactphysics3d::CollisionWorld* world,
                       const std::string& meshPath)
           : openglframework::Mesh(), mVBOVertices(GL_ARRAY_BUFFER),
             mVBONormals(GL_ARRAY_BUFFER), mVBOTextureCoords(GL_ARRAY_BUFFER),
             mVBOIndices(GL_ELEMENT_ARRAY_BUFFER) {

    // Load the mesh from a file
    openglframework::MeshReaderWriter::loadMeshFromFile(meshPath, *this);

    // Calculate the normals of the mesh
    calculateNormals();

    // Initialize the position where the sphere will be rendered
    translateWorld(position);

    // Compute the scaling matrix
    mScalingMatrix = openglframework::Matrix4::identity();


    // Vertex and Indices array for the triangle mesh (data in shared and not copied)
    mPhysicsTriangleVertexArray =
            new rp3d::TriangleVertexArray(getNbVertices(), &(mVertices[0]), sizeof(openglframework::Vector3),
                                          getNbFaces(0), &(mIndices[0][0]), sizeof(int),
                                          rp3d::TriangleVertexArray::VERTEX_FLOAT_TYPE,
                                          rp3d::TriangleVertexArray::INDEX_INTEGER_TYPE);

    // Create the collision shape for the rigid body (convex mesh shape) and
    // do not forget to delete it at the end
    mConvexShape = new rp3d::ConvexMeshShape(mPhysicsTriangleVertexArray);

    // Initial position and orientation of the rigid body
    rp3d::Vector3 initPosition(position.x, position.y, position.z);
    rp3d::Quaternion initOrientation = rp3d::Quaternion::identity();
    rp3d::Transform transform(initPosition, initOrientation);

    mPreviousTransform = transform;

    // Create a rigid body corresponding to the sphere in the dynamics world
    mBody = world->createCollisionBody(transform);

    // Add a collision shape to the body and specify the mass of the collision shape
    mProxyShape = mBody->addCollisionShape(mConvexShape, rp3d::Transform::identity());

    // Create the VBOs and VAO
    createVBOAndVAO();

    mTransformMatrix = mTransformMatrix * mScalingMatrix;
}

// Constructor
ConvexMesh::ConvexMesh(const openglframework::Vector3 &position, float mass,
                       reactphysics3d::DynamicsWorld* dynamicsWorld,
                       const std::string& meshPath)
           : openglframework::Mesh(), mVBOVertices(GL_ARRAY_BUFFER),
             mVBONormals(GL_ARRAY_BUFFER), mVBOTextureCoords(GL_ARRAY_BUFFER),
             mVBOIndices(GL_ELEMENT_ARRAY_BUFFER) {

    // Load the mesh from a file
    openglframework::MeshReaderWriter::loadMeshFromFile(meshPath, *this);

    // Calculate the normals of the mesh
    calculateNormals();

    // Initialize the position where the sphere will be rendered
    translateWorld(position);

    // Compute the scaling matrix
    mScalingMatrix = openglframework::Matrix4::identity();

    // Vertex and Indices array for the triangle mesh (data in shared and not copied)
    mPhysicsTriangleVertexArray =
            new rp3d::TriangleVertexArray(getNbVertices(), &(mVertices[0]), sizeof(openglframework::Vector3),
                                          getNbFaces(0), &(mIndices[0][0]), sizeof(int),
                                          rp3d::TriangleVertexArray::VERTEX_FLOAT_TYPE,
                                          rp3d::TriangleVertexArray::INDEX_INTEGER_TYPE);

    // Create the collision shape for the rigid body (convex mesh shape) and do
    // not forget to delete it at the end
    mConvexShape = new rp3d::ConvexMeshShape(mPhysicsTriangleVertexArray);

    // Initial position and orientation of the rigid body
    rp3d::Vector3 initPosition(position.x, position.y, position.z);
    rp3d::Quaternion initOrientation = rp3d::Quaternion::identity();
    rp3d::Transform transform(initPosition, initOrientation);

    // Create a rigid body corresponding to the sphere in the dynamics world
    rp3d::RigidBody* body = dynamicsWorld->createRigidBody(transform);

    // Add a collision shape to the body and specify the mass of the collision shape
    mProxyShape = body->addCollisionShape(mConvexShape, rp3d::Transform::identity(), mass);

    mBody = body;

    // Create the VBOs and VAO
    createVBOAndVAO();

    mTransformMatrix = mTransformMatrix * mScalingMatrix;
}

// Destructor
ConvexMesh::~ConvexMesh() {

    // Destroy the mesh
    destroy();

    // Destroy the VBOs and VAO
    mVBOIndices.destroy();
    mVBOVertices.destroy();
    mVBONormals.destroy();
    mVBOTextureCoords.destroy();
    mVAO.destroy();

    delete mPhysicsTriangleVertexArray;
    delete mConvexShape;
}

// Render the sphere at the correct position and with the correct orientation
void ConvexMesh::render(openglframework::Shader& shader,
                    const openglframework::Matrix4& worldToCameraMatrix) {

    // Bind the shader
    shader.bind();

    // Set the model to camera matrix
    shader.setMatrix4x4Uniform("localToWorldMatrix", mTransformMatrix);
    shader.setMatrix4x4Uniform("worldToCameraMatrix", worldToCameraMatrix);

    // Set the normal matrix (inverse transpose of the 3x3 upper-left sub matrix of the
    // model-view matrix)
    const openglframework::Matrix4 localToCameraMatrix = worldToCameraMatrix * mTransformMatrix;
    const openglframework::Matrix3 normalMatrix =
                       localToCameraMatrix.getUpperLeft3x3Matrix().getInverse().getTranspose();
    shader.setMatrix3x3Uniform("normalMatrix", normalMatrix, false);

    // Set the vertex color
    openglframework::Color currentColor = mBody->isSleeping() ? mSleepingColor : mColor;
    openglframework::Vector4 color(currentColor.r, currentColor.g, currentColor.b, currentColor.a);
    shader.setVector4Uniform("vertexColor", color, false);

    // Bind the VAO
    mVAO.bind();

    mVBOVertices.bind();

    // Get the location of shader attribute variables
    GLint vertexPositionLoc = shader.getAttribLocation("vertexPosition");
    GLint vertexNormalLoc = shader.getAttribLocation("vertexNormal", false);

    glEnableVertexAttribArray(vertexPositionLoc);
    glVertexAttribPointer(vertexPositionLoc, 3, GL_FLOAT, GL_FALSE, 0, (char*)NULL);

    mVBONormals.bind();

    if (vertexNormalLoc != -1) glVertexAttribPointer(vertexNormalLoc, 3, GL_FLOAT, GL_FALSE, 0, (char*)NULL);
    if (vertexNormalLoc != -1) glEnableVertexAttribArray(vertexNormalLoc);

    // For each part of the mesh
    for (unsigned int i=0; i<getNbParts(); i++) {
        glDrawElements(GL_TRIANGLES, getNbFaces(i) * 3, GL_UNSIGNED_INT, (char*)NULL);
    }

    glDisableVertexAttribArray(vertexPositionLoc);
    if (vertexNormalLoc != -1) glDisableVertexAttribArray(vertexNormalLoc);

    mVBONormals.unbind();
    mVBOVertices.unbind();

    // Unbind the VAO
    mVAO.unbind();

    // Unbind the shader
    shader.unbind();
}

// Create the Vertex Buffer Objects used to render with OpenGL.
/// We create two VBOs (one for vertices and one for indices)
void ConvexMesh::createVBOAndVAO() {

    // Create the VBO for the vertices data
    mVBOVertices.create();
    mVBOVertices.bind();
    size_t sizeVertices = mVertices.size() * sizeof(openglframework::Vector3);
    mVBOVertices.copyDataIntoVBO(sizeVertices, getVerticesPointer(), GL_STATIC_DRAW);
    mVBOVertices.unbind();

    // Create the VBO for the normals data
    mVBONormals.create();
    mVBONormals.bind();
    size_t sizeNormals = mNormals.size() * sizeof(openglframework::Vector3);
    mVBONormals.copyDataIntoVBO(sizeNormals, getNormalsPointer(), GL_STATIC_DRAW);
    mVBONormals.unbind();

    if (hasTexture()) {
        // Create the VBO for the texture co data
        mVBOTextureCoords.create();
        mVBOTextureCoords.bind();
        size_t sizeTextureCoords = mUVs.size() * sizeof(openglframework::Vector2);
        mVBOTextureCoords.copyDataIntoVBO(sizeTextureCoords, getUVTextureCoordinatesPointer(), GL_STATIC_DRAW);
        mVBOTextureCoords.unbind();
    }

    // Create th VBO for the indices data
    mVBOIndices.create();
    mVBOIndices.bind();
    size_t sizeIndices = mIndices[0].size() * sizeof(unsigned int);
    mVBOIndices.copyDataIntoVBO(sizeIndices, getIndicesPointer(), GL_STATIC_DRAW);
    mVBOIndices.unbind();

    // Create the VAO for both VBOs
    mVAO.create();
    mVAO.bind();

    // Bind the VBO of vertices
    mVBOVertices.bind();

    // Bind the VBO of normals
    mVBONormals.bind();

    if (hasTexture()) {
        // Bind the VBO of texture coords
        mVBOTextureCoords.bind();
    }

    // Bind the VBO of indices
    mVBOIndices.bind();

    // Unbind the VAO
    mVAO.unbind();
}

// Reset the transform
void ConvexMesh::resetTransform(const rp3d::Transform& transform) {

    // Reset the transform
    mBody->setTransform(transform);

    mBody->setIsSleeping(false);

    // Reset the velocity of the rigid body
    rp3d::RigidBody* rigidBody = dynamic_cast<rp3d::RigidBody*>(mBody);
    if (rigidBody != NULL) {
        rigidBody->setLinearVelocity(rp3d::Vector3(0, 0, 0));
        rigidBody->setAngularVelocity(rp3d::Vector3(0, 0, 0));
    }

    updateTransform(1.0f);
}

// Set the scaling of the object
void ConvexMesh::setScaling(const openglframework::Vector3& scaling) {

    // Scale the collision shape
    mProxyShape->setLocalScaling(rp3d::Vector3(scaling.x, scaling.y, scaling.z));

    // Scale the graphics object
    mScalingMatrix = openglframework::Matrix4(scaling.x, 0, 0, 0,
                                              0, scaling.y, 0, 0,
                                              0, 0, scaling.z, 0,
                                              0, 0, 0, 1);
}
