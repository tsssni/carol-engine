#include <scene/camera.h>

Carol::Camera::Camera()
{
    DirectX::XMStoreFloat4x4(&mView, DirectX::XMMatrixIdentity());
    DirectX::XMStoreFloat4x4(&mProj, DirectX::XMMatrixIdentity());
}

DirectX::XMVECTOR Carol::Camera::GetPosition() const
{
    return DirectX::XMLoadFloat3(&mPosition);
}

DirectX::XMFLOAT3 Carol::Camera::GetPosition3f() const
{
    return mPosition;
}

void Carol::Camera::SetPosition(float x, float y, float z)
{
    mPosition = { x,y,z };
    mViewDirty = true;
}

void Carol::Camera::SetPosition(const DirectX::XMFLOAT3& v)
{
    mPosition = v;
    mViewDirty = true;
}

DirectX::XMVECTOR Carol::Camera::GetRight() const
{
    return DirectX::XMLoadFloat3(&mRight);
}

DirectX::XMFLOAT3 Carol::Camera::GetRight3f() const
{
    return mRight;
}

DirectX::XMVECTOR Carol::Camera::GetUp() const
{
    return DirectX::XMLoadFloat3(&mUp);
}

DirectX::XMFLOAT3 Carol::Camera::GetUp3f() const
{
    return mUp;
}

DirectX::XMVECTOR Carol::Camera::GetLook() const
{
    return DirectX::XMLoadFloat3(&mLook);
}

DirectX::XMFLOAT3 Carol::Camera::GetLook3f() const
{
    return mLook;
}

void Carol::Camera::LookAt(DirectX::XMVECTOR pos, DirectX::XMVECTOR target, DirectX::XMVECTOR worldUp)
{
    auto look = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(target, pos));
    auto right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(worldUp, look));
    auto up = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(look, right));

    DirectX::XMStoreFloat3(&mPosition, pos);
    DirectX::XMStoreFloat3(&mLook, look);
    DirectX::XMStoreFloat3(&mRight, right);
    DirectX::XMStoreFloat3(&mUp, up);

    mViewDirty = true;
}

void Carol::Camera::LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up)
{
    DirectX::XMVECTOR posVec = DirectX::XMLoadFloat3(&pos);
    DirectX::XMVECTOR targetVec = DirectX::XMLoadFloat3(&target);
    DirectX::XMVECTOR upVec = DirectX::XMLoadFloat3(&up);

    LookAt(posVec, targetVec, upVec);
}

DirectX::XMMATRIX Carol::Camera::GetView() const
{
    return DirectX::XMLoadFloat4x4(&mView);
}

DirectX::XMMATRIX Carol::Camera::GetProj() const
{
    return DirectX::XMLoadFloat4x4(&mProj);
}

DirectX::XMFLOAT4X4 Carol::Camera::GetView4x4f() const
{
    return mView;
}

DirectX::XMFLOAT4X4 Carol::Camera::GetProj4x4f() const
{
    return mProj;
}

void Carol::Camera::Strafe(float d)
{
    DirectX::XMVECTOR strafe = DirectX::XMVectorReplicate(d);
    DirectX::XMVECTOR right = DirectX::XMLoadFloat3(&mRight);
    DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&mPosition);

    DirectX::XMStoreFloat3(&mPosition, DirectX::XMVectorMultiplyAdd(strafe, right, pos));

    mViewDirty = true;
}

void Carol::Camera::Walk(float d)
{
    DirectX::XMVECTOR walk = DirectX::XMVectorReplicate(d);
    DirectX::XMVECTOR look = DirectX::XMLoadFloat3(&mLook);
    DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&mPosition);

    DirectX::XMStoreFloat3(&mPosition, DirectX::XMVectorMultiplyAdd(walk, look, pos));

    mViewDirty = true;
}

void Carol::Camera::Pitch(float angle)
{
    DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationAxis(DirectX::XMLoadFloat3(&mRight), angle);

    DirectX::XMStoreFloat3(&mUp, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mUp), rotation));
    DirectX::XMStoreFloat3(&mLook, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mLook), rotation));

    mViewDirty = true;
}

void Carol::Camera::Roll(float angle)
{
    DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationAxis(DirectX::XMLoadFloat3(&mLook), angle);

    DirectX::XMStoreFloat3(&mUp, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mUp), rotation));
    DirectX::XMStoreFloat3(&mRight, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mRight), rotation));

    mViewDirty = true;
}

void Carol::Camera::Yaw(float angle)
{
    DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationAxis(DirectX::XMLoadFloat3(&mUp), angle);

    DirectX::XMStoreFloat3(&mRight, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mRight), rotation));
    DirectX::XMStoreFloat3(&mLook, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mLook), rotation));

    mViewDirty = true;
}

void Carol::Camera::RotateX(float angle)
{
    DirectX::XMMATRIX R = DirectX::XMMatrixRotationX(angle);

    DirectX::XMStoreFloat3(&mRight, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mRight), R));
    DirectX::XMStoreFloat3(&mUp, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mUp), R));
    DirectX::XMStoreFloat3(&mLook, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mLook), R));

    mViewDirty = true;
}

void Carol::Camera::RotateY(float angle)
{
    DirectX::XMMATRIX R = DirectX::XMMatrixRotationY(angle);

    DirectX::XMStoreFloat3(&mRight, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mRight), R));
    DirectX::XMStoreFloat3(&mUp, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mUp), R));
    DirectX::XMStoreFloat3(&mLook, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mLook), R));

    mViewDirty = true;
}

void Carol::Camera::RotateZ(float angle)
{
    DirectX::XMMATRIX R = DirectX::XMMatrixRotationZ(angle);

    DirectX::XMStoreFloat3(&mRight, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mRight), R));
    DirectX::XMStoreFloat3(&mUp, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mUp), R));
    DirectX::XMStoreFloat3(&mLook, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mLook), R));

    mViewDirty = true;
}

void Carol::Camera::Rotate(DirectX::XMFLOAT3 axis, float angle)
{
    DirectX::XMMATRIX R = DirectX::XMMatrixRotationAxis(DirectX::XMLoadFloat3(&axis), angle);

    DirectX::XMStoreFloat3(&mRight, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mRight), R));
    DirectX::XMStoreFloat3(&mUp, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mUp), R));
    DirectX::XMStoreFloat3(&mLook, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mLook), R));

    mViewDirty = true;
}

void Carol::Camera::UpdateViewMatrix()
{
    if (mViewDirty)
    {
        DirectX::XMVECTOR right = DirectX::XMLoadFloat3(&mRight);
        DirectX::XMVECTOR up = DirectX::XMLoadFloat3(&mUp);
        DirectX::XMVECTOR look = DirectX::XMLoadFloat3(&mLook);
        DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&mPosition);

        look = DirectX::XMVector3Normalize(look);
        up = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(look, right));
        right = DirectX::XMVector3Cross(up, look);

        float oriX = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(pos, right));
        float oriY = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(pos, up));
        float oriZ = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(pos, look));

        DirectX::XMStoreFloat3(&mRight, right);
        DirectX::XMStoreFloat3(&mUp, up);
        DirectX::XMStoreFloat3(&mLook, look);

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

float Carol::Camera::GetNearZ() const
{
    return mNearZ;
}

float Carol::Camera::GetFarZ() const
{
    return mFarZ;
}

float Carol::PerspectiveCamera::GetAspect() const
{
    return mAspect;
}

float Carol::PerspectiveCamera::GetFovY() const
{
    return mFovY;
}

float Carol::PerspectiveCamera::GetFovX() const
{
    float halfWidth = 0.5f * GetNearWindowWidth();
    return 2.0f * atan(halfWidth / mNearZ);
}

float Carol::PerspectiveCamera::GetNearWindowWidth() const
{
    return mAspect * mNearWindowHeight;
}

float Carol::PerspectiveCamera::GetNearWindowHeight() const
{
    return mNearWindowHeight;
}

float Carol::PerspectiveCamera::GetFarWindowWidth() const
{
    return mAspect * mFarWindowHeight;
}

float Carol::PerspectiveCamera::GetFarWindowHeight() const
{
    return mFarWindowHeight;
}

Carol::PerspectiveCamera::PerspectiveCamera()
{
    SetLens(0.25 * DirectX::XM_PI, 1.f, 1.f, 1000.f);
}

Carol::PerspectiveCamera::PerspectiveCamera(float fovY, float aspect, float zn, float zf)
{
    SetLens(fovY, aspect, zn, zf);
}

void Carol::PerspectiveCamera::SetLens(float fovY, float aspect, float zn, float zf)
{
    mFovY = fovY;
    mAspect = aspect;
    mNearZ = zn;
    mFarZ = zf;

    mNearWindowHeight = 2.0f * zn * tan(0.5f * fovY);
    mFarWindowHeight = 2.0f * zf * tan(0.5f * fovY);

    DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(fovY, aspect, zn, zf);
    mBoundingFrustrum = DirectX::BoundingFrustum(proj);

    DirectX::XMStoreFloat4x4(&mProj, proj);
}

bool Carol::PerspectiveCamera::Contains(const DirectX::BoundingBox& boundingBox)const
{
    DirectX::BoundingBox box;
    boundingBox.Transform(box, DirectX::XMLoadFloat4x4(&mView));
    return mBoundingFrustrum.Contains(box) != DirectX::DISJOINT;
}

Carol::OrthographicCamera::OrthographicCamera()
{
    SetLens(140.f, 0.f, 1000.f);
}

Carol::OrthographicCamera::OrthographicCamera(float radius, float zn, float zf)
{
    SetLens(radius, zn, zf);
}

Carol::OrthographicCamera::OrthographicCamera(float width, float height, float zn, float zf)
{
    SetLens(width, height, zn, zf);
}

void Carol::OrthographicCamera::SetLens(float radius, float zn, float zf)
{
    SetLens(radius * 2, radius * 2, zn, zf);
}

void Carol::OrthographicCamera::SetLens(float width, float height, float zn, float zf)
{
    mNearZ = zn;
    mFarZ = zf;

    DirectX::XMMATRIX proj = DirectX::XMMatrixOrthographicLH(width, height, zn, zf);
    DirectX::XMStoreFloat4x4(&mProj, proj);

    mBoundingBox = DirectX::BoundingBox(
        { 0, 0, (zn + zf) / 2 },
        { width / 2,height / 2,(zn + zf) / 2 }
    );
}

bool Carol::OrthographicCamera::Contains(const DirectX::BoundingBox& boundingBox)const
{
    DirectX::BoundingBox box;
    boundingBox.Transform(box, DirectX::XMLoadFloat4x4(&mView));
    return mBoundingBox.Contains(box) != DirectX::DISJOINT;
}
