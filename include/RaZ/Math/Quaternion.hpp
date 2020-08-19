#pragma once

#ifndef RAZ_QUATERNION_HPP
#define RAZ_QUATERNION_HPP

#include "RaZ/Math/Angle.hpp"
#include "RaZ/Math/Matrix.hpp"
#include "RaZ/Math/Vector.hpp"

namespace Raz {

/// Quaternion representing a rotation in 3D space.
/// Quaternions are used to avoid [gimbal locks](https://en.wikipedia.org/wiki/Gimbal_lock), present with Euler angles.
/// \tparam T Type of the values to be held by the quaternion.
template <typename T = float>
class Quaternion {
  static_assert(std::is_floating_point_v<T>, "Error: Quaternion's type must be floating point.");

public:
  constexpr Quaternion(T w, T x, T y, T z) noexcept : m_real{ w }, m_complexes(x, y, z) {}
  constexpr Quaternion(Radians<T> angle, const Vec3<T>& axis) noexcept;
  constexpr Quaternion(Radians<T> angle, T axisX, T axisY, T axisZ) noexcept : Quaternion(angle, Vec3<T>(axisX, axisY, axisZ)) {}
  constexpr Quaternion(const Quaternion&) noexcept = default;
  constexpr Quaternion(Quaternion&&) noexcept = default;

  /// Creates a quaternion representing an identity transformation.
  /// \return Identity quaternion.
  static constexpr Quaternion<T> identity() noexcept { return Quaternion<T>(1, 0, 0, 0); }

  /// Computes the norm of the quaternion.
  /// Calculating the actual norm requires a square root operation to be involved, which is expensive.
  /// As such, this function should be used if actual length is needed; otherwise, prefer computeSquaredNorm().
  /// \return Quaternion's norm.
  constexpr T computeNorm() const { return std::sqrt(computeSquaredNorm()); }
  /// Computes the squared norm of the quaternion.
  /// The squared norm is equal to the addition of all components (real & complexes alike) squared.
  /// This calculation does not involve a square root; it is then to be preferred over computeNorm() for faster operations.
  /// \return Quaternion's squared norm.
  constexpr T computeSquaredNorm() const noexcept { return (m_real * m_real + m_complexes.computeSquaredLength()); }
  /// Computes the normalized quaternion to make it a unit one.
  /// A unit quaternion is also called a [versor](https://en.wikipedia.org/wiki/Versor).
  /// \return Normalized quaternion.
  constexpr Quaternion<T> normalize() const noexcept;
  /// Computes the conjugate of the quaternion.
  /// A quaternion's conjugate is simply computed by multiplying the complex components by -1.
  /// \return Quaternion's conjugate.
  constexpr Quaternion<T> conjugate() const noexcept;
  /// Computes the inverse (or reciprocal) of the quaternion.
  /// Inversing a quaternion consists of dividing the components of the conjugate by the squared norm.
  /// \return Quaternion's inverse.
  constexpr Quaternion<T> inverse() const noexcept;
  /// Computes the rotation matrix represented by the quaternion.
  /// This operation automatically scales the matrix so that it returns a unit one.
  /// \return Rotation matrix.
  constexpr Mat4<T> computeMatrix() const;

  /// Default copy assignment operator.
  /// \return Reference to the copied quaternion.
  constexpr Quaternion& operator=(const Quaternion&) noexcept = default;
  /// Default move assignment operator.
  /// \return Reference to the moved quaternion.
  constexpr Quaternion& operator=(Quaternion&&) noexcept = default;
  /// Quaternions multiplication.
  /// \param quat Quaternion to be multiplied by.
  /// \return Result of the multiplied quaternions.
  constexpr Quaternion operator*(const Quaternion& quat) const noexcept;
  /// Quaternions multiplication.
  /// \param quat Quaternion to be multiplied by.
  /// \return Reference to the modified original quaternion.
  constexpr Quaternion& operator*=(const Quaternion& quat) noexcept;
  /// Matrix conversion operator; computes the rotation matrix represented by the quaternion.
  /// \return Rotation matrix.
  constexpr operator Mat4<T>() const { return computeMatrix(); }

private:
  T m_real {};
  Vec3<T> m_complexes {};
};

using Quaternionf = Quaternion<float>;
using Quaterniond = Quaternion<double>;

} // namespace Raz

#include "RaZ/Math/Quaternion.inl"

#endif // RAZ_QUATERNION_HPP
