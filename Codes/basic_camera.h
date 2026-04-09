#ifndef basic_camera_h
#define basic_camera_h

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>

class BasicCamera {
public:
    // Camera Attributes
    glm::vec3 eye;
    glm::vec3 lookAt;   // Fixed pivot for Orbiting
    glm::vec3 WorldUp;

    glm::vec3 Front;
    glm::vec3 Right;
    glm::vec3 Up;

    // Euler Angles
    float Yaw;
    float Pitch;
    float Roll;

    // Camera Options
    float Zoom;
    float MovementSpeed;
    float RotationSpeed;  // For key-based rotation
    float OrbitSpeed;     // For orbiting
    float MouseSensitivity; //For mouse movement

    // Constructor
    BasicCamera(
        float eyeX = 0.0f, float eyeY = 6.0f, float eyeZ = 6.0f,
        float lookAtX = 0.0f, float lookAtY = 0.0f, float lookAtZ = 0.0f,
        glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f)
    ) {
        eye = glm::vec3(eyeX, eyeY, eyeZ);
        lookAt = glm::vec3(lookAtX, lookAtY, lookAtZ);
        WorldUp = worldUp;

        // Default:
        Yaw = -135.0f;
        Pitch = -35.0f;
        Roll = 0.0f;

        MovementSpeed = 4.5f;
        RotationSpeed = 70.0f;
        OrbitSpeed = 60.0f;
        Zoom = 45.0f;
        MouseSensitivity = 0.1f; 

        updateCameraVectors();
    }

    glm::mat4 createViewMatrix()
    {
        return glm::lookAt(eye, eye + Front, Up);
    }

    glm::mat4 createViewMatrix2()
    {
        glm::vec3 target = eye + Front;
        //Use lookAt or target
        glm::vec3 zaxis = glm::normalize(eye - lookAt);
        glm::vec3 xaxis = glm::normalize(glm::cross(glm::normalize(WorldUp), zaxis));
        glm::vec3 yaxis = glm::cross(zaxis, xaxis);

        glm::mat4 translation(1.0f);
        translation[3][0] = -eye.x;
        translation[3][1] = -eye.y;
        translation[3][2] = -eye.z;

        glm::mat4 rotation(1.0f);
        rotation[0][0] = xaxis.x;
        rotation[1][0] = xaxis.y;
        rotation[2][0] = xaxis.z;

        rotation[0][1] = yaxis.x;
        rotation[1][1] = yaxis.y;
        rotation[2][1] = yaxis.z;

        rotation[0][2] = zaxis.x;
        rotation[1][2] = zaxis.y;
        rotation[2][2] = zaxis.z;

        return rotation * translation;
    }

    //Flying Movement

    void MoveForward(float dt) { eye += Front * (MovementSpeed * dt); }
    void MoveBackward(float dt) { eye -= Front * (MovementSpeed * dt); }
    void MoveRight(float dt) { eye += Right * (MovementSpeed * dt); }
    void MoveLeft(float dt) { eye -= Right * (MovementSpeed * dt); }
    void MoveUp(float dt) { eye += Up * (MovementSpeed * dt); }
    void MoveDown(float dt) { eye -= Up * (MovementSpeed * dt); }

    //Rotation
    void AddPitch(float deg) {
        Pitch += deg;
        if (Pitch > 89.0f) Pitch = 89.0f;
        if (Pitch < -89.0f) Pitch = -89.0f;
        updateCameraVectors();
    }

    void AddYaw(float deg) {
        Yaw += deg;
        updateCameraVectors();
    }

    void AddRoll(float deg) {
        Roll += deg;
        updateCameraVectors();
    }

    // MOUSE: Look Around 
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true)
    {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Yaw += xoffset;
        Pitch += yoffset;

        if (constrainPitch)
        {
            if (Pitch > 89.0f)
                Pitch = 89.0f;
            if (Pitch < -89.0f)
                Pitch = -89.0f;
        }
        updateCameraVectors();
    }

    // Orbit around
    void OrbitAroundLookAt(float dt) {
        float angle = glm::radians(OrbitSpeed * dt);
        glm::vec3 offset = eye - lookAt;
        glm::mat4 rot = glm::rotate(glm::mat4(1.0f), angle, WorldUp);
        offset = glm::vec3(rot * glm::vec4(offset, 1.0f));
        eye = lookAt + offset;

        // Keep looking at target
        Front = glm::normalize(lookAt - eye);
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up = glm::normalize(glm::cross(Right, Front));

        applyRoll();
    }

    //Zoom
    void ProcessMouseScroll(float yoffset) {
        Zoom -= (float)yoffset;
        if (Zoom < 1.0f) Zoom = 1.0f;
        if (Zoom > 45.0f) Zoom = 45.0f;
    }

    void SetPresetView(int viewId, float dist = 20.0f)
    {
        Roll = 0.0f;
        switch (viewId) {
        case 1: // TOP
            eye = lookAt + glm::vec3(0.0f, dist, 0.0f); Yaw = 90.0f; Pitch = -89.0f; break;
        case 2: // BOTTOM
            eye = lookAt + glm::vec3(0.0f, -dist, 0.0f); Yaw = -90.0f; Pitch = 89.0f; break;
        case 3: // FRONT
            eye = lookAt + glm::vec3(0.0f, 0.0f, dist); Yaw = -90.0f; Pitch = 0.0f; break;
        case 4: // BACK
            eye = lookAt + glm::vec3(0.0f, 0.0f, -dist); Yaw = 90.0f; Pitch = 0.0f; break;
        case 5: // RIGHT
            eye = lookAt + glm::vec3(dist, 0.0f, 0.0f); Yaw = 180.0f; Pitch = 0.0f; break;
        case 6: // LEFT
            eye = lookAt + glm::vec3(-dist, 0.0f, 0.0f); Yaw = 0.0f; Pitch = 0.0f; break;
        }
        updateCameraVectors();
    }

    void update() {
		updateCameraVectors();
    }

private:
    void applyRoll() {
        if (Roll == 0.0f) return;
        glm::mat4 rollMat = glm::rotate(glm::mat4(1.0f), glm::radians(Roll), Front);
        Up = glm::normalize(glm::vec3(rollMat * glm::vec4(Up, 0.0f)));
        Right = glm::normalize(glm::cross(Up, Front));
    }

    void updateCameraVectors() {
        // Calculate the new Front vector
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);

        // Also re-calculate the Right and Up vector
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up = glm::normalize(glm::cross(Right, Front));

        applyRoll();
    }
};

#endif /* basic_camera_h */