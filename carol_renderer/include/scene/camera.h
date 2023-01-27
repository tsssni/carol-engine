#pragma once
#include <DirectXMath.h>
#include <DirectXCollision.h>

namespace Carol
{
	class Camera
	{
	public:

		Camera();
		void SetLens(float fovY, float aspect, float zn, float zf);

		virtual bool Contains(DirectX::BoundingBox boundingBox);

		DirectX::XMVECTOR GetPosition()const;
		DirectX::XMFLOAT3 GetPosition3f()const;
		void SetPosition(float x, float y, float z);
		void SetPosition(const DirectX::XMFLOAT3& v);

		float GetAspect()const;
		float GetFovY()const;
		float GetFovX()const;

		float GetNearZ()const;
		float GetFarZ()const;
		float GetNearWindowWidth()const;
		float GetNearWindowHeight()const;
		float GetFarWindowWidth()const;
		float GetFarWindowHeight()const;
		
		DirectX::XMVECTOR GetRight()const;
		DirectX::XMFLOAT3 GetRight3f()const;
		DirectX::XMVECTOR GetUp()const;
		DirectX::XMFLOAT3 GetUp3f()const;
		DirectX::XMVECTOR GetLook()const;
		DirectX::XMFLOAT3 GetLook3f()const;

		void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);
		void LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up);

		DirectX::XMMATRIX GetView()const;
		DirectX::XMMATRIX GetProj()const;

		DirectX::XMFLOAT4X4 GetView4x4f()const;
		DirectX::XMFLOAT4X4 GetProj4x4f()const;

		void Strafe(float d);
		void Walk(float d);

		void Roll(float angle);
		void Pitch(float angle);
		void Yaw(float angle);

		void RotateX(float angle);
		void RotateY(float angle);
		void RotateZ(float angle);
		void Rotate(DirectX::XMFLOAT3 axis, float angle);

		void UpdateViewMatrix();

	protected:
		DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };
		DirectX::XMFLOAT3 mLook = { 0.0f, 0.0f, 1.0f };

		float mAspect = 0.0f;
		float mFovY = 0.0f;
		float mNearWindowHeight = 0.0f;
		float mFarWindowHeight = 0.0f;
		float mNearZ = 0.0f;
		float mFarZ = 0.0f;
		bool mViewDirty = true;

		DirectX::XMFLOAT4X4 mView;
		DirectX::XMFLOAT4X4 mProj;

		DirectX::BoundingFrustum mBoundingFrustrum;
	};

	class OrthographicCamera : public Camera
	{
	public:
		OrthographicCamera(DirectX::XMFLOAT3 viewDir, DirectX::XMFLOAT3 targetPos, float radius);
		void SetLens(DirectX::XMFLOAT3 viewDir3f, DirectX::XMFLOAT3 targetPos3f, float radius);
		virtual bool Contains(DirectX::BoundingBox boundingBox);

	protected:
		DirectX::BoundingBox mBoundingBox;
	};
}


