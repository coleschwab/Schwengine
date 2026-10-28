// Vector2.h : Include file for standard system include files,
// or project specific include files.

#pragma once
#include <iostream>
#include <cmath>
struct Vector2 {
	float x, y;

	// Constructor
	Vector2(float _x = 0.0f, float _y = 0.0f) : x(_x), y(_y) {}

	// Operator Overloading
	Vector2 operator+(const Vector2& v) const {
		return Vector2(x + v.x, y + v.y);
	}

	//Vector Subtraction
	Vector2 operator-(const Vector2& v) const {
		return Vector2(x - v.x, y - v.y);
	}

	//Scalar multiplication
	Vector2 operator*(float scalar) const {
		return Vector2(x * scalar, y * scalar);
	}

	// Dot product
	float dot(const Vector2& v) const {
		return (x * v.x) + (y * v.y);
	}

	// Cross product
	float cross(const Vector2& v) const {
		return (x * v.y) - (y * v.x);
	}

	//The Constants
	float lengthSquared() const {
		return (x * x) + (y * y);
	}
};