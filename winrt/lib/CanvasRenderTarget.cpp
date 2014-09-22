// Copyright (c) Microsoft Corporation. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may
// not use these files except in compliance with the License. You may obtain
// a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.

#include "pch.h"
#include "CanvasRenderTarget.h"
#include "CanvasDevice.h"
#include "CanvasDrawingSession.h"
#include "CanvasBitmap.h"

namespace ABI { namespace Microsoft { namespace Graphics { namespace Canvas
{
    using namespace ::Microsoft::WRL::Wrappers;

    //
    // CanvasRenderTargetManager
    //

    ComPtr<CanvasRenderTarget> CanvasRenderTargetManager::CreateNew(
        ICanvasDevice* canvasDevice,
        ABI::Windows::Foundation::Size size)
    {
        ComPtr<ICanvasDeviceInternal> canvasDeviceInternal;
        ThrowIfFailed(canvasDevice->QueryInterface(canvasDeviceInternal.GetAddressOf()));

        auto d2dBitmap = canvasDeviceInternal->CreateBitmap(size);

        return Make<CanvasRenderTarget>(shared_from_this(), d2dBitmap.Get(), canvasDevice);
    }


    ComPtr<CanvasRenderTarget> CanvasRenderTargetManager::CreateWrapper(
        ID2D1Bitmap1* d2dBitmap)
    {
        // TODO #2473: Need to create CanvasBitmap or CanvasRenderTarget as
        // appropriate, based on the d2dBitmap

        // !! How do we get a device from a bitmap? !!
        ThrowHR(E_NOTIMPL);

#if WE_KNOW_HOW_TO_GET_DEVICE_FROM_BITMAP
        auto renderTarget = Make<CanvasRenderTarget>(
            shared_from_this(),
            d2dBitmap,
            nullptr /*HOW DO WE GET A DEVICE FROM A BITMAP??*/ );
        CheckMakeResult(renderTarget);
        
        return renderTarget;
#endif
    }


    //
    // CanvasRenderTargetFactory
    //


    CanvasRenderTargetFactory::CanvasRenderTargetFactory()
    {
    }


    std::shared_ptr<CanvasRenderTargetManager> CanvasRenderTargetFactory::CreateManager()
    {
        return std::make_shared<CanvasRenderTargetManager>();
    }


    IFACEMETHODIMP CanvasRenderTargetFactory::Create(
        ICanvasResourceCreator *resourceCreator,
        ABI::Windows::Foundation::Size sizeInPixels,
        ICanvasRenderTarget **renderTarget)
    {
        return ExceptionBoundary(
            [&]
            {
                CheckInPointer(resourceCreator);
                CheckAndClearOutPointer(renderTarget);

                ComPtr<ICanvasDevice> canvasDevice;
                ThrowIfFailed(resourceCreator->get_Device(&canvasDevice));

                auto bitmap = GetManager()->Create(canvasDevice.Get(), sizeInPixels);
                CheckMakeResult(bitmap);

                ThrowIfFailed(bitmap.CopyTo(renderTarget));
            });
    }


    IFACEMETHODIMP CanvasRenderTargetFactory::GetOrCreate(
        IUnknown* resource,
        IInspectable** wrapper)
    {
        return ExceptionBoundary(
            [&]
            {
                CheckInPointer(resource);
                CheckAndClearOutPointer(wrapper);

                ComPtr<ID2D1Bitmap1> d2dBitmap;
                ThrowIfFailed(resource->QueryInterface(d2dBitmap.GetAddressOf()));

                auto newCanvasRenderTarget = GetManager()->GetOrCreate(d2dBitmap.Get());

                ThrowIfFailed(newCanvasRenderTarget.CopyTo(wrapper));
            });
    }


    //
    // CanvasBitmapDrawingSessionAdapter
    //


    class CanvasBitmapDrawingSessionAdapter : public ICanvasDrawingSessionAdapter
    {
        ComPtr<ID2D1DeviceContext1> m_d2dDeviceContext;

    public:
        CanvasBitmapDrawingSessionAdapter(ID2D1DeviceContext1* d2dDeviceContext)
            : m_d2dDeviceContext(d2dDeviceContext) 
        {
            d2dDeviceContext->BeginDraw();
        }

        virtual D2D1_POINT_2F GetRenderingSurfaceOffset() override
        {
            return D2D1::Point2F(0, 0);
        }

        virtual void EndDraw() override
        {
            ThrowIfFailed(m_d2dDeviceContext->EndDraw());
        }
    };

    static ComPtr<ICanvasDrawingSession> CreateDrawingSessionOverD2DBitmap(
        ICanvasDevice* owner,
        ID2D1Bitmap1* targetBitmap)
    {
        assert(owner != nullptr);
        assert(targetBitmap != nullptr);

        //
        // Create a new ID2D1DeviceContext
        //
        ComPtr<ICanvasDeviceInternal> deviceInternal;
        ThrowIfFailed(owner->QueryInterface(deviceInternal.GetAddressOf()));
        auto d2dDevice = deviceInternal->GetD2DDevice();

        ComPtr<ID2D1DeviceContext1> deviceContext;
        ThrowIfFailed(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &deviceContext));

        //
        // Set the target
        //
        deviceContext->SetTarget(targetBitmap);

        auto adapter = std::make_shared<CanvasBitmapDrawingSessionAdapter>(deviceContext.Get());

        auto drawingSessionManager = CanvasDrawingSessionFactory::GetOrCreateManager();
        return drawingSessionManager->Create(owner, deviceContext.Get(), adapter);
    }

    //
    // CanvasRenderTarget
    //


    CanvasRenderTarget::CanvasRenderTarget(
        std::shared_ptr<CanvasRenderTargetManager> manager,
        ID2D1Bitmap1* bitmap,
        ICanvasDevice* canvasDevice)
        : CanvasBitmapImpl(manager, bitmap)
        , m_device(canvasDevice)
    {
    }


    IFACEMETHODIMP CanvasRenderTarget::CreateDrawingSession(
        _COM_Outptr_ ICanvasDrawingSession** drawingSession)
    {
        return ExceptionBoundary(
            [&]
            {
                CheckAndClearOutPointer(drawingSession);
                
                auto resource = GetD2DBitmap();

                auto newDrawingSession = CreateDrawingSessionOverD2DBitmap(
                    m_device.Get(),
                    resource.Get());

                ThrowIfFailed(newDrawingSession.CopyTo(drawingSession));
            });
    }

    ActivatableClassWithFactory(CanvasRenderTarget, CanvasRenderTargetFactory);
}}}}