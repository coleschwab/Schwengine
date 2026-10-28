#pragma once
#include <iostream>
#include <cmath>
struct Vector4 {
	float x, y, z, w;

	// Constructor
	Vector4(float _x = 0.0f, float _y = 0.0f, float _z = 0.0f, float _w = 1.0f) : x(_x), y(_y), z(_z), w(_w) {}

	// Operator Overloading
	Vector4 operator+(const Vector4& v) const {
		return Vector4(x + v.x, y + v.y, z + v.z, w + v.w);
	}

	//Vector Subtraction
	Vector4 operator-(const Vector4& v) const {
		return Vector4(x - v.x, y - v.y, z - v.z, w - v.w);
	}

	//Scalar multiplication
	Vector4 operator*(float scalar) const {
		return Vector4(x * scalar, y * scalar, z * scalar, w * scalar);
	}

	// Dot product
	float dot(const Vector4& v) const {
		return (x * v.x) + (y * v.y) + (z * v.z) + (w * v.w);
	}

	// Cross product
	Vector4 cross(const Vector4& v) const {
		return Vector4(
			(y * v.z) - (z * v.y),
			(z * v.x) - (x * v.z),
			(x * v.y) - (y * v.x),
			0.0f
		);
	}

	//The Constants
	float lengthSquared() const {
		return (x * x) + (y * y) + (z * z);
	};

	float length() const {
		return std::sqrtf(lengthSquared());
	}

	Vector4 normalize() const {
		float len = length();
		if (len > 0.0f) {
			float invLen = 1.0f / len;
			return Vector4(x * invLen, y * invLen, z * invLen, w);
		}
		return Vector4(0.0f, 0.0f, 0.0f, w);
	}
	
};