#include <scene/camera.h>

namespace Carol {
    using namespace DirectX;
}

Carol::Camera::Camera()
{
    XMStoreFloat4x4(&mView, XMMatrixIdentity());
    XMStoreFloat4x4(&mProj, XMMatrixIdentity());
}

Carol::XMVECTOR Carol::Camera::GetPosition() const
{
    return XMLoadFloat3(&mPosition);
}

Carol::XMFLOAT3 Carol::Camera::GetPosition3f() const
{
    return mPosition;
}

void Carol::Camera::SetPosition(float x, float y, float z)
{
    mPosition = { x,y,z };
    mViewDirty = true;
}

void Carol::Camera::SetPosition(const XMFLOAT3& v)
{
    mPosition = v;
    mViewDirty = true;
}

Carol::XMVECTOR Carol::Camera::GetRight() const
{
    return XMLoadFloat3(&mRight);
}

Carol::XMFLOAT3 Carol::Camera::GetRight3f() const
{
    return mRight;
}

Carol::XMVECTOR Carol::Camera::GetUp() const
{
    return XMLoadFloat3(&mUp);
}

Carol::XMFLOAT3 Carol::Camera::GetUp3f() const
{
    return mUp;
}

Carol::XMVECTOR Carol::Camera::GetLook() const
{
    return XMLoadFloat3(&mLook);
}

Carol::XMFLOAT3 Carol::Camera::GetLook3f() const
{
    return mLook;
}

void Carol::Camera::LookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp)
{
    auto look = XMVector3Normalize(XMVectorSubtract(target, pos));
    auto right = XMVector3Normalize(XMVector3Cross(worldUp, look));
    auto up = XMVector3Normalize(XMVector3Cross(look, right));

    XMStoreFloat3(&mPosition, pos);
    XMStoreFloat3(&mLook, look);
    XMStoreFloat3(&mRight, right);
    XMStoreFloat3(&mUp, up);

    mViewDirty = true;
}

void Carol::Camera::LookAt(const XMFLOAT3& pos, const XMFLOAT3& target, const XMFLOAT3& up)
{
    XMVECTOR posVec = XMLoadFloat3(&pos);
    XMVECTOR targetVec = XMLoadFloat3(&target);
    XMVECTOR upVec = XMLoadFloat3(&up);

    LookAt(posVec, targetVec, upVec);
}

Carol::XMMATRIX Carol::Camera::GetView() const
{
    return XMLoadFloat4x4(&mView);
}

Carol::XMMATRIX Carol::Camera::GetProj() const
{
    return XMLoadFloat4x4(&mProj);
}

Carol::XMFLOAT4X4 Carol::Camera::GetView4x4f() const
{
    return mView;
}

Carol::XMFLOAT4X4 Carol::Camera::GetProj4x4f() const
{
    return mProj;
}

void Carol::Camera::Strafe(float d)
{
    XMVECTOR strafe = XMVectorReplicate(d);
    XMVECTOR right = XMLoadFloat3(&mRight);
    XMVECTOR pos = XMLoadFloat3(&mPosition);

    XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(strafe, right, pos));

    mViewDirty = true;
}

void Carol::Camera::Walk(float d)
{
    XMVECTOR walk = XMVectorReplicate(d);
    XMVECTOR look = XMLoadFloat3(&mLook);
    XMVECTOR pos = XMLoadFloat3(&mPosition);

    XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(walk, look, pos));

    mViewDirty = true;
}

void Carol::Camera::Pitch(float angle)
{
    XMMATRIX rotation = XMMatrixRotationAxis(XMLoadFloat3(&mRight), angle);

    XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), rotation));
    XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), rotation));

    mViewDirty = true;
}

void Carol::Camera::Roll(float angle)
{
    XMMATRIX rotation = XMMatrixRotationAxis(XMLoadFloat3(&mLook), angle);

    XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), rotation));
    XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), rotation));

    mViewDirty = true;
}

void Carol::Camera::Yaw(float angle)
{
    XMMATRIX rotation = XMMatrixRotationAxis(XMLoadFloat3(&mUp), angle);

    XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), rotation));
    XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), rotation));

    mViewDirty = true;
}

void Carol::Camera::RotateX(float angle)
{
    XMMATRIX R = XMMatrixRotationX(angle);

    XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
    XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
    XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

    mViewDirty = true;
}

void Carol::Camera::RotateY(float angle)
{
    XMMATRIX R = XMMatrixRotationY(angle);

    XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
    XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
    XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

    mViewDirty = true;
}

void Carol::Camera::RotateZ(float angle)
{
    XMMATRIX R = XMMatrixRotationZ(angle);

    XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
    XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
    XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

    mViewDirty = true;
}

void Carol::Camera::Rotate(XMFLOAT3 axis, float angle)
{
    XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&axis), angle);

    XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
    XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
    XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

    mViewDirty = true;
}

void Carol::Camera::UpdateViewMatrix()
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
    SetLens(0.25 * XM_PI, 1.f, 1.f, 1000.f);
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

    XMMATRIX proj = XMMatrixPerspectiveFovLH(fovY, aspect, zn, zf);
    mBoundingFrustrum = BoundingFrustum(proj);

    XMStoreFloat4x4(&mProj, proj);
}

bool Carol::PerspectiveCamera::Contains(const DirectX::BoundingBox& boundingBox)const
{
    BoundingBox box;
    boundingBox.Transform(box, XMLoadFloat4x4(&mView));
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

    XMMATRIX proj = XMMatrixOrthographicLH(width, height, zn, zf);
    XMStoreFloat4x4(&mProj, proj);

    mBoundingBox = BoundingBox(
        { 0, 0, (zn + zf) / 2 },
        { width / 2,height / 2,(zn + zf) / 2 }
    );
}

bool Carol::OrthographicCamera::Contains(const DirectX::BoundingBox& boundingBox)const
{
    BoundingBox box;
    boundingBox.Transform(box, XMLoadFloat4x4(&mView));
    return mBoundingBox.Contains(box) != DirectX::DISJOINT;
}
