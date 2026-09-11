#include "location_geolocator.h"

#ifndef _AMD64_
#define _AMD64_
#endif

#include <winapifamily.h>
#undef WINAPI_FAMILY
#define WINAPI_FAMILY WINAPI_FAMILY_PC_APP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef CINTERFACE
#define CINTERFACE
#endif
#ifndef COBJMACROS
#define COBJMACROS
#endif

#include <windows.h>
#include <winstring.h>
#include <inspectable.h>
#include <asyncinfo.h>

extern "C" {
typedef enum RO_INIT_TYPE {
    RO_INIT_SINGLETHREADED = 0,
    RO_INIT_MULTITHREADED = 1,
} RO_INIT_TYPE;

HRESULT WINAPI RoInitialize(RO_INIT_TYPE initType);
HRESULT WINAPI RoActivateInstance(HSTRING activatableClassId, IInspectable** instance);
HRESULT WINAPI RoGetActivationFactory(HSTRING activatableClassId, REFIID iid, void** factory);
}

typedef enum GeolocationAccessStatus {
    GeolocationAccessStatus_Unspecified = 0,
    GeolocationAccessStatus_Allowed = 1,
    GeolocationAccessStatus_Denied = 2,
} GeolocationAccessStatus;

typedef interface IGeoAsyncOp IGeoAsyncOp;
typedef interface IGeoAccessAsyncOp IGeoAccessAsyncOp;
typedef interface IGeolocator IGeolocator;
typedef interface IGeolocatorStatics IGeolocatorStatics;
typedef interface IGeoposition IGeoposition;
typedef interface IGeocoordinate IGeocoordinate;

typedef struct IGeoAsyncOpVtbl {
    BEGIN_INTERFACE
    HRESULT(STDMETHODCALLTYPE* QueryInterface)(IGeoAsyncOp* This, REFIID riid, void** ppvObject);
    ULONG(STDMETHODCALLTYPE* AddRef)(IGeoAsyncOp* This);
    ULONG(STDMETHODCALLTYPE* Release)(IGeoAsyncOp* This);
    HRESULT(STDMETHODCALLTYPE* GetIids)(IGeoAsyncOp* This, ULONG* iidCount, IID** iids);
    HRESULT(STDMETHODCALLTYPE* GetRuntimeClassName)(IGeoAsyncOp* This, HSTRING* className);
    HRESULT(STDMETHODCALLTYPE* GetTrustLevel)(IGeoAsyncOp* This, TrustLevel* trustLevel);
    HRESULT(STDMETHODCALLTYPE* put_Completed)(IGeoAsyncOp* This, void* handler);
    HRESULT(STDMETHODCALLTYPE* get_Completed)(IGeoAsyncOp* This, void** handler);
    HRESULT(STDMETHODCALLTYPE* GetResults)(IGeoAsyncOp* This, IGeoposition** result);
    END_INTERFACE
} IGeoAsyncOpVtbl;

interface IGeoAsyncOp {
    CONST_VTBL struct IGeoAsyncOpVtbl* lpVtbl;
};

typedef struct IGeoAccessAsyncOpVtbl {
    BEGIN_INTERFACE
    HRESULT(STDMETHODCALLTYPE* QueryInterface)(IGeoAccessAsyncOp* This, REFIID riid, void** ppvObject);
    ULONG(STDMETHODCALLTYPE* AddRef)(IGeoAccessAsyncOp* This);
    ULONG(STDMETHODCALLTYPE* Release)(IGeoAccessAsyncOp* This);
    HRESULT(STDMETHODCALLTYPE* GetIids)(IGeoAccessAsyncOp* This, ULONG* iidCount, IID** iids);
    HRESULT(STDMETHODCALLTYPE* GetRuntimeClassName)(IGeoAccessAsyncOp* This, HSTRING* className);
    HRESULT(STDMETHODCALLTYPE* GetTrustLevel)(IGeoAccessAsyncOp* This, TrustLevel* trustLevel);
    HRESULT(STDMETHODCALLTYPE* put_Completed)(IGeoAccessAsyncOp* This, void* handler);
    HRESULT(STDMETHODCALLTYPE* get_Completed)(IGeoAccessAsyncOp* This, void** handler);
    HRESULT(STDMETHODCALLTYPE* GetResults)(IGeoAccessAsyncOp* This, GeolocationAccessStatus* result);
    END_INTERFACE
} IGeoAccessAsyncOpVtbl;

interface IGeoAccessAsyncOp {
    CONST_VTBL struct IGeoAccessAsyncOpVtbl* lpVtbl;
};

typedef struct IGeolocatorStaticsVtbl {
    BEGIN_INTERFACE
    HRESULT(STDMETHODCALLTYPE* QueryInterface)(IGeolocatorStatics* This, REFIID riid, void** ppvObject);
    ULONG(STDMETHODCALLTYPE* AddRef)(IGeolocatorStatics* This);
    ULONG(STDMETHODCALLTYPE* Release)(IGeolocatorStatics* This);
    HRESULT(STDMETHODCALLTYPE* GetIids)(IGeolocatorStatics* This, ULONG* iidCount, IID** iids);
    HRESULT(STDMETHODCALLTYPE* GetRuntimeClassName)(IGeolocatorStatics* This, HSTRING* className);
    HRESULT(STDMETHODCALLTYPE* GetTrustLevel)(IGeolocatorStatics* This, TrustLevel* trustLevel);
    HRESULT(STDMETHODCALLTYPE* RequestAccessAsync)(IGeolocatorStatics* This, IGeoAccessAsyncOp** result);
    HRESULT(STDMETHODCALLTYPE* GetGeopositionHistoryAsync)(IGeolocatorStatics* This, INT64 startTime, void** result);
    HRESULT(STDMETHODCALLTYPE* GetGeopositionHistoryWithDurationAsync)(IGeolocatorStatics* This, INT64 startTime, INT64 duration, void** result);
    END_INTERFACE
} IGeolocatorStaticsVtbl;

interface IGeolocatorStatics {
    CONST_VTBL struct IGeolocatorStaticsVtbl* lpVtbl;
};

typedef struct IGeolocatorVtbl {
    BEGIN_INTERFACE
    HRESULT(STDMETHODCALLTYPE* QueryInterface)(IGeolocator* This, REFIID riid, void** ppvObject);
    ULONG(STDMETHODCALLTYPE* AddRef)(IGeolocator* This);
    ULONG(STDMETHODCALLTYPE* Release)(IGeolocator* This);
    HRESULT(STDMETHODCALLTYPE* GetIids)(IGeolocator* This, ULONG* iidCount, IID** iids);
    HRESULT(STDMETHODCALLTYPE* GetRuntimeClassName)(IGeolocator* This, HSTRING* className);
    HRESULT(STDMETHODCALLTYPE* GetTrustLevel)(IGeolocator* This, TrustLevel* trustLevel);
    HRESULT(STDMETHODCALLTYPE* get_DesiredAccuracy)(IGeolocator* This, INT32* value);
    HRESULT(STDMETHODCALLTYPE* put_DesiredAccuracy)(IGeolocator* This, INT32 value);
    HRESULT(STDMETHODCALLTYPE* get_MovementThreshold)(IGeolocator* This, DOUBLE* value);
    HRESULT(STDMETHODCALLTYPE* put_MovementThreshold)(IGeolocator* This, DOUBLE value);
    HRESULT(STDMETHODCALLTYPE* get_ReportInterval)(IGeolocator* This, UINT32* value);
    HRESULT(STDMETHODCALLTYPE* put_ReportInterval)(IGeolocator* This, UINT32 value);
    HRESULT(STDMETHODCALLTYPE* get_LocationStatus)(IGeolocator* This, INT32* value);
    HRESULT(STDMETHODCALLTYPE* GetGeopositionAsync)(IGeolocator* This, IGeoAsyncOp** value);
    HRESULT(STDMETHODCALLTYPE* GetGeopositionAsyncWithAgeAndTimeout)(IGeolocator* This, INT64 maximumAge, INT64 timeout, IGeoAsyncOp** value);
    HRESULT(STDMETHODCALLTYPE* add_PositionChanged)(IGeolocator* This, void* handler, INT64* token);
    HRESULT(STDMETHODCALLTYPE* remove_PositionChanged)(IGeolocator* This, INT64 token);
    HRESULT(STDMETHODCALLTYPE* add_StatusChanged)(IGeolocator* This, void* handler, INT64* token);
    HRESULT(STDMETHODCALLTYPE* remove_StatusChanged)(IGeolocator* This, INT64 token);
    END_INTERFACE
} IGeolocatorVtbl;

interface IGeolocator {
    CONST_VTBL struct IGeolocatorVtbl* lpVtbl;
};

typedef struct IGeopositionVtbl {
    BEGIN_INTERFACE
    HRESULT(STDMETHODCALLTYPE* QueryInterface)(IGeoposition* This, REFIID riid, void** ppvObject);
    ULONG(STDMETHODCALLTYPE* AddRef)(IGeoposition* This);
    ULONG(STDMETHODCALLTYPE* Release)(IGeoposition* This);
    HRESULT(STDMETHODCALLTYPE* GetIids)(IGeoposition* This, ULONG* iidCount, IID** iids);
    HRESULT(STDMETHODCALLTYPE* GetRuntimeClassName)(IGeoposition* This, HSTRING* className);
    HRESULT(STDMETHODCALLTYPE* GetTrustLevel)(IGeoposition* This, TrustLevel* trustLevel);
    HRESULT(STDMETHODCALLTYPE* get_Coordinate)(IGeoposition* This, IGeocoordinate** value);
    HRESULT(STDMETHODCALLTYPE* get_CivicAddress)(IGeoposition* This, void** value);
    END_INTERFACE
} IGeopositionVtbl;

interface IGeoposition {
    CONST_VTBL struct IGeopositionVtbl* lpVtbl;
};

typedef struct IGeocoordinateVtbl {
    BEGIN_INTERFACE
    HRESULT(STDMETHODCALLTYPE* QueryInterface)(IGeocoordinate* This, REFIID riid, void** ppvObject);
    ULONG(STDMETHODCALLTYPE* AddRef)(IGeocoordinate* This);
    ULONG(STDMETHODCALLTYPE* Release)(IGeocoordinate* This);
    HRESULT(STDMETHODCALLTYPE* GetIids)(IGeocoordinate* This, ULONG* iidCount, IID** iids);
    HRESULT(STDMETHODCALLTYPE* GetRuntimeClassName)(IGeocoordinate* This, HSTRING* className);
    HRESULT(STDMETHODCALLTYPE* GetTrustLevel)(IGeocoordinate* This, TrustLevel* trustLevel);
    HRESULT(STDMETHODCALLTYPE* get_Latitude)(IGeocoordinate* This, DOUBLE* value);
    HRESULT(STDMETHODCALLTYPE* get_Longitude)(IGeocoordinate* This, DOUBLE* value);
    HRESULT(STDMETHODCALLTYPE* get_Altitude)(IGeocoordinate* This, void** value);
    HRESULT(STDMETHODCALLTYPE* get_Accuracy)(IGeocoordinate* This, DOUBLE* value);
    HRESULT(STDMETHODCALLTYPE* get_AltitudeAccuracy)(IGeocoordinate* This, void** value);
    HRESULT(STDMETHODCALLTYPE* get_Heading)(IGeocoordinate* This, void** value);
    HRESULT(STDMETHODCALLTYPE* get_Speed)(IGeocoordinate* This, void** value);
    HRESULT(STDMETHODCALLTYPE* get_Timestamp)(IGeocoordinate* This, INT64* value);
    END_INTERFACE
} IGeocoordinateVtbl;

interface IGeocoordinate {
    CONST_VTBL struct IGeocoordinateVtbl* lpVtbl;
};

static const IID IID_IGeolocatorStatics = {
    0x9a8e7571, 0x2df5, 0x4591, {0x9f, 0x87, 0xeb, 0x5f, 0xd8, 0x94, 0xe9, 0xb7}};
static const IID IID_IGeolocator = {
    0xa9c3bf62, 0x4524, 0x4989, {0x8a, 0xa9, 0xde, 0x01, 0x9d, 0x2e, 0x55, 0x1f}};
static const IID IID_IAsyncInfoLocal = {
    0x00000036, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

namespace {

bool EnsureRoInitialized() {
    static bool initialized = false;
    if (initialized) {
        return true;
    }

    const HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return false;
    }

    initialized = true;
    return true;
}

bool WaitForAsync(IAsyncInfo* asyncInfo, DWORD timeoutMs = 60000) {
    if (!asyncInfo) {
        return false;
    }

    const DWORD start = GetTickCount();
    AsyncStatus status = Started;
    while (status == Started) {
        if (GetTickCount() - start > timeoutMs) {
            return false;
        }
        if (FAILED(IAsyncInfo_get_Status(asyncInfo, &status))) {
            return false;
        }
        if (status == Started) {
            Sleep(50);
        }
    }

    return status == Completed;
}

template <typename AsyncOperation>
bool WaitForAsyncOperation(AsyncOperation* operation, DWORD timeoutMs = 60000) {
    if (!operation) {
        return false;
    }

    IAsyncInfo* asyncInfo = nullptr;
    const HRESULT hr = operation->lpVtbl->QueryInterface(
        operation, IID_IAsyncInfoLocal, reinterpret_cast<void**>(&asyncInfo));
    if (FAILED(hr) || !asyncInfo) {
        return false;
    }

    const bool completed = WaitForAsync(asyncInfo, timeoutMs);
    IAsyncInfo_Release(asyncInfo);
    return completed;
}

bool RequestGeolocationAccess(HSTRING className) {
    IGeolocatorStatics* statics = nullptr;
    HRESULT hr = RoGetActivationFactory(
        className, IID_IGeolocatorStatics, reinterpret_cast<void**>(&statics));
    if (FAILED(hr) || !statics) {
        return false;
    }

    IGeoAccessAsyncOp* accessOp = nullptr;
    hr = statics->lpVtbl->RequestAccessAsync(statics, &accessOp);
    statics->lpVtbl->Release(statics);
    if (FAILED(hr) || !accessOp) {
        return false;
    }

    if (!WaitForAsyncOperation(accessOp)) {
        accessOp->lpVtbl->Release(accessOp);
        return false;
    }

    GeolocationAccessStatus accessStatus = GeolocationAccessStatus_Unspecified;
    hr = accessOp->lpVtbl->GetResults(accessOp, &accessStatus);
    accessOp->lpVtbl->Release(accessOp);
    if (FAILED(hr)) {
        return false;
    }

    return accessStatus == GeolocationAccessStatus_Allowed;
}

}  // namespace

bool TryWinRtGeolocator(HWND, double& latitude, double& longitude, bool requestAccess) {
    if (!EnsureRoInitialized()) {
        return false;
    }

    HSTRING className = nullptr;
    const HRESULT createHr = WindowsCreateString(
        L"Windows.Devices.Geolocation.Geolocator",
        static_cast<UINT32>(wcslen(L"Windows.Devices.Geolocation.Geolocator")),
        &className);
    if (FAILED(createHr) || !className) {
        return false;
    }

    if (requestAccess && !RequestGeolocationAccess(className)) {
        WindowsDeleteString(className);
        return false;
    }

    IInspectable* instance = nullptr;
    HRESULT hr = RoActivateInstance(className, &instance);
    WindowsDeleteString(className);
    if (FAILED(hr) || !instance) {
        return false;
    }

    IGeolocator* geolocator = nullptr;
    hr = instance->lpVtbl->QueryInterface(instance, IID_IGeolocator, reinterpret_cast<void**>(&geolocator));
    instance->lpVtbl->Release(instance);
    if (FAILED(hr) || !geolocator) {
        return false;
    }

    IGeoAsyncOp* positionOp = nullptr;
    hr = geolocator->lpVtbl->GetGeopositionAsync(geolocator, &positionOp);
    geolocator->lpVtbl->Release(geolocator);
    if (FAILED(hr) || !positionOp) {
        return false;
    }

    if (!WaitForAsyncOperation(positionOp, 120000)) {
        positionOp->lpVtbl->Release(positionOp);
        return false;
    }

    IGeoposition* geoposition = nullptr;
    hr = positionOp->lpVtbl->GetResults(positionOp, &geoposition);
    positionOp->lpVtbl->Release(positionOp);
    if (FAILED(hr) || !geoposition) {
        return false;
    }

    IGeocoordinate* coordinate = nullptr;
    hr = geoposition->lpVtbl->get_Coordinate(geoposition, &coordinate);
    geoposition->lpVtbl->Release(geoposition);
    if (FAILED(hr) || !coordinate) {
        return false;
    }

    DOUBLE lat = 0.0;
    DOUBLE lon = 0.0;
    hr = coordinate->lpVtbl->get_Latitude(coordinate, &lat);
    if (SUCCEEDED(hr)) {
        hr = coordinate->lpVtbl->get_Longitude(coordinate, &lon);
    }
    coordinate->lpVtbl->Release(coordinate);
    if (FAILED(hr)) {
        return false;
    }

    latitude = lat;
    longitude = lon;
    return true;
}
