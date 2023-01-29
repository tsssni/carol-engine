export module carol.renderer.scene.camera;
import <DirectXMath.h>;
import <DirectXCollision.h>;

namespace Carol
{
    using namespace DirectX;

    export class Camera
	{
	public:
		Camera()
        {
            XMStoreFloat4x4(&mView, XMMatrixIdentity());
            XMStoreFloat4x4(&mProj, XMMatrixIdentity());
            SetLens(0.25*XM_PI, 1.0f, 1.0f, 1000.0f);
        }

		void SetLens(float fovY, float aspect, float zn, float zf)
        {
            mFovY = fovY;
            mAspect = aspect;
            mNearZ = zn;
            mFarZ = zf;

            mNearWindowHeight = 2.0f * zn * tan(0.5f * fovY);
            mFarWindowHeight = 2.0f * zf * tan(0.5f * fovY);

            XMMATRIX proj = XMMatrixPerspectiveFovLH(fovY, aspect, zn, zf);
            mBoundingFrustrum = BoundingFrustum(proj);

            XMStoreFloat4x4(&mProj, proj);
        }

		virtual bool Contains(BoundingBox boundingBox)
        {
            boundingBox.Transform(boundingBox, XMLoadFloat4x4(&mView));
            return mBoundingFrustrum.Contains(boundingBox) != DirectX::DISJOINT;
        }

		XMVECTOR GetPosition()const
        {
            return XMLoadFloat3(&mPosition);
        }

		XMFLOAT3 GetPosition3f()const
        {
            return mPosition;
        }

		void SetPosition(float x, float y, float z)
        {
            mPosition = { x,y,z };
            mViewDirty = true;
        }

		void SetPosition(const XMFLOAT3& v)
        {
            mPosition = v;
            mViewDirty = true;
        }

		float GetAspect()const
        {
            return mAspect;
        }

		float GetFovY()const
        {
            return mFovY;
        }

		float GetFovX()const
        {
            float halfWidth = 0.5f * GetNearWindowWidth();
            return 2.0f * atan(halfWidth / mNearZ);
        }

		float GetNearZ()const
        {
            return mNearZ;
        }

		float GetFarZ()const
        {
            return mFarZ;
        }

		float GetNearWindowWidth()const
        {
            return mAspect * mNearWindowHeight;
        }

		float GetNearWindowHeight()const
        {
            return mNearWindowHeight;
        }

		float GetFarWindowWidth()const
        {
            return mAspect * mFarWindowHeight;
        }

		float GetFarWindowHeight()const
        {
            return mFarWindowHeight;
        }
		
		XMVECTOR GetRight()const
        {
            return XMLoadFloat3(&mRight);
        }

		XMFLOAT3 GetRight3f()const
        {
            return mRight;
        }

		XMVECTOR GetUp()const
        {
            return XMLoadFloat3(&mUp);
        }

		XMFLOAT3 GetUp3f()const
        {
            return mUp;
        }

		XMVECTOR GetLook()const
        {
            return XMLoadFloat3(&mLook);
        }

		XMFLOAT3 GetLook3f()const
        {
            return mLook;
        }

		void LookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp)
        {
            auto look = XMVector3Normalize(XMVectorSubtract(pos, target));
            auto right = XMVector3Normalize(XMVector3Cross(worldUp, look));
            auto up = XMVector3Normalize(XMVector3Cross(look, right));

            XMStoreFloat3(&mPosition, pos);
            XMStoreFloat3(&mLook, look);
            XMStoreFloat3(&mRight, right);
            XMStoreFloat3(&mUp, up);

            mViewDirty = true;
        }

		void LookAt(const XMFLOAT3& pos, const XMFLOAT3& target, const XMFLOAT3& up)
        {
            XMVECTOR posVec = XMLoadFloat3(&pos);
            XMVECTOR targetVec = XMLoadFloat3(&target);
            XMVECTOR upVec = XMLoadFloat3(&up);

            LookAt(posVec, targetVec, upVec);
        }

		XMMATRIX GetView()const
        {
            return XMLoadFloat4x4(&mView);
        }

		XMFLOAT4X4 GetView4x4f()const
        {
            return mView;
        }

		XMMATRIX GetProj()const
        {
            return XMLoadFloat4x4(&mProj);
        }

		XMFLOAT4X4 GetProj4x4f()const
        {
            return mProj;
        }

		void Strafe(float d)
        {
            XMVECTOR strafe = XMVectorReplicate(d);
            XMVECTOR right = XMLoadFloat3(&mRight);
            XMVECTOR pos = XMLoadFloat3(&mPosition);

            XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(strafe, right, pos));

            mViewDirty = true;
        }

		void Walk(float d)
        {
            XMVECTOR walk = XMVectorReplicate(d);
            XMVECTOR look = XMLoadFloat3(&mLook);
            XMVECTOR pos = XMLoadFloat3(&mPosition);

            XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(walk, look, pos));

            mViewDirty = true;
        }

		void Roll(float angle)
        {
            XMMATRIX rotation = XMMatrixRotationAxis(XMLoadFloat3(&mLook), angle);

            XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), rotation));
            XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), rotation));

            mViewDirty = true;
        }

		void Pitch(float angle)
        {
            XMMATRIX rotation = XMMatrixRotationAxis(XMLoadFloat3(&mRight), angle);

            XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), rotation));
            XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), rotation));

            mViewDirty = true;
        }

		void Yaw(float angle)
        {
            XMMATRIX rotation = XMMatrixRotationAxis(XMLoadFloat3(&mUp), angle);

            XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), rotation));
            XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), rotation));

            mViewDirty = true;
        }

		void RotateX(float angle)
        {
            XMMATRIX R = XMMatrixRotationX(angle);

            XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
            XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
            XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

            mViewDirty = true;
        }

		void RotateY(float angle)
        {
            XMMATRIX R = XMMatrixRotationY(angle);

            XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
            XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
            XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

            mViewDirty = true;
        }

		void RotateZ(float angle)
        {
            XMMATRIX R = XMMatrixRotationZ(angle);

            XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
            XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
            XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

            mViewDirty = true;
        }

		void Rotate(XMFLOAT3 axis, float angle)
        {
    XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&axis), angle);

    XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
    XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
    XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

    mViewDirty = true;
}

		void UpdateViewMatrix()
        {
            if (mViewDirty)
            {
                XMVECTOR right = XMLoadFloat3(&mRight);
                XMVECTOR up = XMLoadFloat3(&mUp);
                XMVECTOR look = XMLoadFloat3(&mLook);
                XMVECTOR pos = XMLoadFloat3(&mPosition);

                look = XMVector3Normalize(look);
                up = XMVector3Normalize(XMVector3Cross(look, right));
                right = XMVector3Cross(up, look);

                float oriX = -XMVectorGetX(XMVector3Dot(pos, right));
                float oriY = -XMVectorGetX(XMVector3Dot(pos, up));
                float oriZ = -XMVectorGetX(XMVector3Dot(pos, look));

                XMStoreFloat3(&mRight, right);
                XMStoreFloat3(&mUp, up);
                XMStoreFloat3(&mLook, look);

                mView(0, 0) = mRight.x;
                mView(1, 0) = mRight.y;
                mView(2, 0) = mRight.z;
                mView(3, 0) = oriX;

                mView(0, 1) = mUp.x;
                mView(1, 1) = mUp.y;
                mView(2, 1) = mUp.z;
                mView(3, 1) = oriY;

                mView(0, 2) = mLook.x;
                mView(1, 2) = mLook.y;
                mView(2, 2) = mLook.z;
                mView(3, 2) = oriZ;

                mView(0, 3) = 0.0f;
                mView(1, 3) = 0.0f;
                mView(2, 3) = 0.0f;
                mView(3, 3) = 1.0f;

                mViewDirty = false;
            }
        }

	protected:
		XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
		XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
		XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };
		XMFLOAT3 mLook = { 0.0f, 0.0f, 1.0f };

		float mAspect = 0.0f;
		float mFovY = 0.0f;
		float mNearWindowHeight = 0.0f;
		float mFarWindowHeight = 0.0f;
		float mNearZ = 0.0f;
		float mFarZ = 0.0f;
		bool mViewDirty = true;

		XMFLOAT4X4 mView;
		XMFLOAT4X4 mProj;

		BoundingFrustum mBoundingFrustrum;
	};

	export class OrthographicCamera : public Camera
	{
	public:
		OrthographicCamera(XMFLOAT3 viewDir, XMFLOAT3 targetPos, float radius)
        {
            SetLens(viewDir, targetPos, radius);
        }

		void SetLens(XMFLOAT3 viewDir3f, XMFLOAT3 targetPos3f, float radius)
        {
            XMVECTOR viewDir = XMLoadFloat3(&viewDir3f);
            XMVECTOR targetPos = XMLoadFloat3(&targetPos3f);
            XMVECTOR viewPos = targetPos - 2.0f * radius * viewDir;
            XMMATRIX view = XMMatrixLookAtLH(viewPos, targetPos, { 0.0f,1.0f,0.0f,0.0f });
            XMStoreFloat4x4(&mView, view);

            XMFLOAT3 rectCenter;
            XMStoreFloat3(&rectCenter, XMVector3TransformCoord(targetPos, view));

            float l = rectCenter.x - radius;
            float b = rectCenter.y - radius;
            float n = rectCenter.z - radius;
            float r = rectCenter.x + radius;
            float t = rectCenter.y + radius;
            float f = rectCenter.z + radius;
            
            mNearZ = n;
            mFarZ = f;
            XMMATRIX proj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);
            XMStoreFloat4x4(&mProj, proj);

            mBoundingBox = BoundingBox(
                { (l + r) / 2,(b + t) / 2,(n + f) / 2 },
                { (r - l) / 2,(t - b) / 2,(f - n) / 2 }
            );
        }

		virtual bool Contains(BoundingBox boundingBox)
        {
            boundingBox.Transform(boundingBox, XMLoadFloat4x4(&mView));
            return mBoundingBox.Contains(boundingBox) != DirectX::DISJOINT;
        }

	protected:
		BoundingBox mBoundingBox;
	};
}