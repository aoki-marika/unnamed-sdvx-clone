#include "stdafx.h"
#include "Camera.hpp"
#include "Application.hpp"
#include "Track.hpp"

Camera::Camera()
{

}
Camera::~Camera()
{

}
void Camera::Tick(float deltaTime, class BeatmapPlayback& playback)
{
	const TimingPoint& currentTimingPoint = playback.GetCurrentTimingPoint();
	//Calculate camera roll
	//Follow the laser track exactly but with a roll speed limit
	float rollSpeedLimit = (m_rollIntensity / (currentTimingPoint.beatDuration / 1000.0f)) * deltaTime;
	rollSpeedLimit /= m_targetRoll != 0.0f ? 1.0f : 2.0f;

	float rollDelta = m_targetRoll - m_roll;
	rollSpeedLimit *= Math::Sign(rollDelta);
	m_roll += (abs(rollDelta) < abs(rollSpeedLimit)) ? rollDelta : rollSpeedLimit;

	if(!rollKeep)
	{
		m_targetRollSet = false;
		m_targetRoll = 0.0f;
	}

	// Update camera shake effects
	m_shakeOffset = Vector3(0.0f);
	for(auto it = m_shakeEffects.begin(); it != m_shakeEffects.end();)
	{
		if(it->time <= 0.0f)
		{
			it = m_shakeEffects.erase(it);
			continue;
		}

		it->time -= deltaTime;
		it->offsets += Vector3(deltaTime);
		float shakeIntensity = it->time / it->duration;
		Vector3 shakeVec = it->amplitude;
		Vector3 shakeInput = it->frequency * it->offsets;
		shakeVec = Vector3(cos(shakeInput.x), cos(shakeInput.y), cos(shakeInput.z)) * shakeVec;
		m_shakeOffset += shakeVec * shakeIntensity;
		it++;
	}
}
void Camera::AddCameraShake(CameraShake cameraShake)
{
	// Randomize offsets
	cameraShake.offsets = Vector3(
		Random::FloatRange(0.0f, 1.0f / cameraShake.frequency.x),
		Random::FloatRange(0.0f, 1.0f / cameraShake.frequency.y),
		Random::FloatRange(0.0f, 1.0f / cameraShake.frequency.z));
	m_shakeEffects.Add(cameraShake);
}
void Camera::AddRollImpulse(float dir, float strength)
{
	m_rollVelocity += dir * strength;
}

void Camera::SetRollIntensity(float val)
{
	m_rollIntensity = val;
}

Vector2 Camera::Project(const Vector3& pos)
{
	Vector3 cameraSpace = m_rsLast.cameraTransform.TransformPoint(pos);
	Vector3 screenSpace = m_rsLast.projectionTransform.TransformPoint(cameraSpace);
	screenSpace.y = -screenSpace.y;
	screenSpace *= 0.5f;
	screenSpace += Vector2(0.5f, 0.5f);
	screenSpace *= m_rsLast.viewportSize;
	return screenSpace.xy();
}

RenderState Camera::CreateRenderState(bool clipped)
{
	// Extension of clipping planes in outward direction
	float viewRangeExtension = clipped ? 0.0f : 5.0f;

	RenderState rs = g_application->GetRenderStateBase();
	
	// Tilt, Height and Near calculated from zoom values
	float base_pitch = -33.0f * pow(1.33f, -zoomTop);
	float base_radius = 4.f * cameraHeightBase * pow(1.1f, -zoomBottom * 3.0f);

	float targetHeight = base_radius * sin(Math::degToRad * base_pitch);
	float targetNear = base_radius * cos(Math::degToRad * base_pitch);

	Transform cameraTransform;
	cameraTransform *= Transform::Rotation({ 0.0f, 0.0f, m_roll * 360.0f });
	cameraTransform *= Transform::Rotation({base_pitch - 40.0f, 0.0f, 0.0f });
	cameraTransform *= Transform::Translation(m_shakeOffset + Vector3( 0.0f, -targetHeight, -targetNear));

	// Calculate clipping distances
	Vector3 cameraPos = cameraTransform.TransformDirection(-Vector3(cameraTransform.GetPosition()));
	Vector3 cameraDir = cameraTransform.TransformDirection(Vector3(0.0f, 0.0f, 1.0f));
	float offset = VectorMath::Dot(cameraPos, cameraDir);

	// Dot products of start and end of track on the viewing direction to get the near and far clipping planes
	float d0 = VectorMath::Dot(Vector3(0.0f, 0.0f, 0.0f), cameraDir) + offset;
	float d1 = VectorMath::Dot(Vector3(0.0f, track->trackLength, 0.0f), cameraDir) + offset;

	rs.cameraTransform = cameraTransform;
	rs.projectionTransform = ProjectionMatrix::CreatePerspective(90.0f, g_aspectRatio, Math::Max(0.2f, d0 - viewRangeExtension), d1 + viewRangeExtension);

	m_rsLast = rs;

	return rs;
}

void Camera::SetTargetRoll(float target)
{
	float actualTarget = target * m_rollIntensity;
	if(!rollKeep)
	{
		m_targetRoll = actualTarget;
		m_targetRollSet = true;
	}
	else
	{
		m_targetRoll = m_roll;
		if (m_roll == 0.0f || Math::Sign(m_roll) == Math::Sign(actualTarget))
		{
			if (m_roll == 0)
				m_targetRoll = actualTarget;
			if (m_roll < 0 && actualTarget < m_roll)
				m_targetRoll = actualTarget;
			else if (actualTarget > m_roll)
				m_targetRoll = actualTarget;
		}
		m_targetRollSet = true;
	}
}

void Camera::SetLasersActive(bool lasersActive)
{
	m_lasersActive = lasersActive;
}

float Camera::GetRoll() const
{
	return m_roll;
}

float Camera::m_ClampRoll(float in) const
{
	float ain = abs(in);
	if(ain < 1.0f)
		return in;
	bool odd = ((uint32)abs(in) % 2) == 1;
	float sign = Math::Sign(in);
	if(odd)
	{
		// Swap sign and modulo
		return -sign * (1.0f-fmodf(ain, 1.0f));
	}
	else
	{
		// Keep sign and modulo
		return sign * fmodf(ain, 1.0f);
	}
}

CameraShake::CameraShake(float duration) : duration(duration)
{
	time = duration;
}
CameraShake::CameraShake(float duration, float amplitude, float freq) : duration(duration), amplitude(amplitude), frequency(freq)
{
	time = duration;
}

