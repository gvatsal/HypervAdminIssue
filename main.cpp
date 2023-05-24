#include <iostream>
#include <format>
#include <filesystem>
#include <fstream>

#include <boost/json.hpp>

#include <Windows.h>
#include <wil/resource.h>
#include <ComputeNetwork.h>
#include <computecore.h>
#include <HyperVDeviceVirtualization.h>
#include <rpc.h>

#pragma region Utils

boost::json::value xjsonReadFromFile(std::filesystem::path filePath)
{
    std::ifstream ifs;
    try
    {
        ifs.exceptions(std::ios::badbit | std::ios::failbit);
        ifs.open(filePath);
    }
    catch (std::exception& exc)
    {
        std::cout << std::format("{}: failed to open file: filePath {}, exc {}", __func__, filePath.string(), exc.what()) << "\n";
        return boost::json::value{};
    }

    try
    {
        std::stringstream ss;
        ss << ifs.rdbuf();
        boost::json::value jv = boost::json::parse(ss.str());
        return jv;
    }
    catch (std::exception& exc)
    {
        std::cout << std::format("{}: failed to parse file: filePath {}, exc {}", __func__, filePath.string(), exc.what()) << "\n";
        return boost::json::value{};
    }
}

template<typename Index>
inline const boost::json::value& operator/ (const boost::json::value& jv, Index index)
{
    if constexpr (std::is_integral_v<Index>) return jv.as_array().at(index);
    else return jv.as_object().at(index);
}

template<typename Index>
inline boost::json::value& operator/ (boost::json::value& jv, Index index)
{
    if constexpr (std::is_integral_v<Index>) return jv.as_array().at(index);
    else return jv.as_object().at(index);
}

std::string xstrUtf8(const wchar_t* wstr)
{
    if (wstr == nullptr)
        return "(nullptr)";

    int length = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    std::string str(static_cast<size_t>(length) - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str.data(), length, NULL, NULL);
    return str;
}

std::wstring xstrUtf16(const char* str)
{
    if (str == nullptr)
        return L"(nullptr)";

    int length = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    std::wstring wstr(static_cast<size_t>(length) - 1, '\0');
    MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr.data(), length);
    return wstr;
}

std::wstring xstrUtf16(const std::string& str)
{
    return xstrUtf16(str.data());
}

std::string xstrUtf8(const boost::json::value& jv)
{
    return (std::stringstream() << jv).str();
}

std::wstring xstrUtf16(const boost::json::value& jv)
{
    return xstrUtf16(xstrUtf8(jv));
}

#pragma endregion

struct VmmgrHypervApi
{
    static void init();
    static inline decltype(&::HcnOpenNetwork) HcnOpenNetwork{ nullptr };
    static inline decltype(&::HcnCloseNetwork) HcnCloseNetwork{ nullptr };
    static inline decltype(&::HcnCreateNetwork) HcnCreateNetwork{ nullptr };

    static inline decltype(&::HcnCloseEndpoint) HcnCloseEndpoint{ nullptr };
    static inline decltype(&::HcnCreateEndpoint) HcnCreateEndpoint{ nullptr };
    static inline decltype(&::HcnDeleteEndpoint) HcnDeleteEndpoint{ nullptr };

    static inline decltype(&::HcsCloseOperation) HcsCloseOperation{ nullptr };
    static inline decltype(&::HcsCloseComputeSystem) HcsCloseComputeSystem{ nullptr };
    static inline decltype(&::HcsCreateOperation) HcsCreateOperation{ nullptr };
    static inline decltype(&::HcsCreateComputeSystem) HcsCreateComputeSystem{ nullptr };
    static inline decltype(&::HcsWaitForOperationResult) HcsWaitForOperationResult{ nullptr };

    static inline decltype(&::HdvInitializeDeviceHost) HdvInitializeDeviceHost{ nullptr };
    static inline decltype(&::HdvTeardownDeviceHost) HdvTeardownDeviceHost{ nullptr };
};

void VmmgrHypervApi::init()
{
    HMODULE mComputeNetworkHandle = LoadLibrary(L"ComputeNetwork.dll");
    if (mComputeNetworkHandle != nullptr)
    {
        void* symbolAddress = GetProcAddress((HMODULE)mComputeNetworkHandle, "HcnOpenNetwork");
        if (symbolAddress != nullptr)
            HcnOpenNetwork = (decltype(&::HcnOpenNetwork))symbolAddress;

        symbolAddress = GetProcAddress((HMODULE)mComputeNetworkHandle, "HcnCloseNetwork");
        if (symbolAddress != nullptr)
            HcnCloseNetwork = (decltype(&::HcnCloseNetwork))symbolAddress;

        symbolAddress = GetProcAddress((HMODULE)mComputeNetworkHandle, "HcnCreateNetwork");
        if (symbolAddress != nullptr)
            HcnCreateNetwork = (decltype(&::HcnCreateNetwork))symbolAddress;


        symbolAddress = GetProcAddress((HMODULE)mComputeNetworkHandle, "HcnCloseEndpoint");
        if (symbolAddress != nullptr)
            HcnCloseEndpoint = (decltype(&::HcnCloseEndpoint))symbolAddress;

        symbolAddress = GetProcAddress((HMODULE)mComputeNetworkHandle, "HcnCreateEndpoint");
        if (symbolAddress != nullptr)
            HcnCreateEndpoint = (decltype(&::HcnCreateEndpoint))symbolAddress;

        symbolAddress = GetProcAddress((HMODULE)mComputeNetworkHandle, "HcnDeleteEndpoint");
        if (symbolAddress != nullptr)
            HcnDeleteEndpoint = (decltype(&::HcnDeleteEndpoint))symbolAddress;
    }

    HMODULE mComputeCoreHandle = LoadLibrary(L"ComputeCore.dll");
    if (mComputeCoreHandle != nullptr)
    {
        void* symbolAddress = GetProcAddress((HMODULE)mComputeCoreHandle, "HcsCloseOperation");
        if (symbolAddress != nullptr)
            HcsCloseOperation = (decltype(&::HcsCloseOperation))symbolAddress;

        symbolAddress = GetProcAddress((HMODULE)mComputeCoreHandle, "HcsCloseComputeSystem");
        if (symbolAddress != nullptr)
            HcsCloseComputeSystem = (decltype(&::HcsCloseComputeSystem))symbolAddress;

        symbolAddress = GetProcAddress((HMODULE)mComputeCoreHandle, "HcsCreateOperation");
        if (symbolAddress != nullptr)
            HcsCreateOperation = (decltype(&::HcsCreateOperation))symbolAddress;

        symbolAddress = GetProcAddress((HMODULE)mComputeCoreHandle, "HcsCreateComputeSystem");
        if (symbolAddress != nullptr)
            HcsCreateComputeSystem = (decltype(&::HcsCreateComputeSystem))symbolAddress;

        symbolAddress = GetProcAddress((HMODULE)mComputeCoreHandle, "HcsWaitForOperationResult");
        if (symbolAddress != nullptr)
            HcsWaitForOperationResult = (decltype(&::HcsWaitForOperationResult))symbolAddress;
    }

    HMODULE mVmDeviceHostHandle = LoadLibrary(L"VmDeviceHost.dll");
    if (mVmDeviceHostHandle != nullptr)
    {
        void* symbolAddress = GetProcAddress((HMODULE)mVmDeviceHostHandle, "HdvInitializeDeviceHost");
        if (symbolAddress != nullptr)
            HdvInitializeDeviceHost = (decltype(&::HdvInitializeDeviceHost))symbolAddress;

        symbolAddress = GetProcAddress((HMODULE)mVmDeviceHostHandle, "HdvTeardownDeviceHost");
        if (symbolAddress != nullptr)
            HdvTeardownDeviceHost = (decltype(&::HdvTeardownDeviceHost))symbolAddress;
    }
}

boost::json::value mAndroidJson;
wil::unique_any < HCN_NETWORK, decltype(&::HcnCloseNetwork), [](HCN_NETWORK h) { return VmmgrHypervApi::HcnCloseNetwork(h); } > mHcnNetwork;
wil::unique_any < HCN_ENDPOINT, decltype(&::HcnCloseEndpoint), [](HCN_ENDPOINT h) { return VmmgrHypervApi::HcnCloseEndpoint(h); } > mHcnEndpoint;
wil::unique_any < HCS_SYSTEM, decltype(&::HcsCloseComputeSystem), [](HCS_SYSTEM h) { return VmmgrHypervApi::HcsCloseComputeSystem(h); } > mHcsSystem;
wil::unique_any < HDV_HOST, decltype(&::HdvTeardownDeviceHost), [](HDV_HOST h) { return VmmgrHypervApi::HdvTeardownDeviceHost(h); } > mHdvHost;

void configureHcnNetwork()
{
    std::string networkGuid = (mAndroidJson / "HcnNetwork" / "ID").as_string().data();
    GUID guidNetwork;

    if (UuidFromStringA((RPC_CSTR)networkGuid.data(), &guidNetwork) != RPC_S_OK)
    {
        std::cout << std::format("{} - Failed to parse Network guid: {}", __func__, networkGuid) << "\n";
    }

    wil::unique_cotaskmem_string errStr;
    HRESULT result = VmmgrHypervApi::HcnOpenNetwork(guidNetwork, &mHcnNetwork, &errStr);
    std::cout << std::format("{} - HcnOpenNetwork:\nresult {}\nerrStr {}", __func__, result, xstrUtf8(errStr.get())) << "\n";

    if (result == HCN_E_NETWORK_NOT_FOUND)
    {
        result = VmmgrHypervApi::HcnCreateNetwork(
            guidNetwork,                                    // Id
            xstrUtf16(mAndroidJson / "HcnNetwork").data(),  // Settings
            &mHcnNetwork,                                   // Network
            &errStr                                         // ErrorRecord
        );
        std::cout << std::format("{} - HcnCreateNetwork\nresult {}\nerrStr {}", __func__, result, xstrUtf8(errStr.get())) << "\n";
    }
}

void configureHcnEndpoint()
{
    std::string endpointGuid = (mAndroidJson / "HcsSystem" / "VirtualMachine" / "Devices" /
        "NetworkAdapters" / "default" / "EndpointId").as_string().data();

    GUID guidEndpoint;
    if (UuidFromStringA((RPC_CSTR)endpointGuid.data(), &guidEndpoint) != RPC_S_OK)
    {
        std::cout << std::format("{} - Failed to parse Endpoint guid: {}", __func__, endpointGuid);
    }

    wil::unique_cotaskmem_string errStr;
    HRESULT result = VmmgrHypervApi::HcnDeleteEndpoint(guidEndpoint, &errStr);
    std::cout << std::format("{} - HcnDeleteEndpoint:\nresult {}\nerrStr {}", __func__, result, xstrUtf8(errStr.get())) << "\n";

    result = VmmgrHypervApi::HcnCreateEndpoint(
        mHcnNetwork.get(),                                  // Network
        guidEndpoint,                                       // Id
        xstrUtf16(mAndroidJson / "HcnEndpoint").data(),     // Settings
        &mHcnEndpoint,                                      // Endpoint
        &errStr);                                           // ErrorRecord
    std::cout << std::format("{} - HcnCreateEndpoint\nresult {}\nerrStr {}", __func__, result, xstrUtf8(errStr.get())) << "\n";
}

void configureHcsSystem()
{
    wil::unique_any < HCS_OPERATION, decltype(&::HcsCloseOperation), [](HCS_OPERATION h) { return VmmgrHypervApi::HcsCloseOperation(h); } > hcsOperation(VmmgrHypervApi::HcsCreateOperation(nullptr, nullptr));
    std::cout << std::format("{} - HcsCreateOperation:\nresult {}", __func__, hcsOperation == NULL ? "failed" : "success") << "\n";

    HRESULT result = VmmgrHypervApi::HcsCreateComputeSystem(
        xstrUtf16("Nougat64_nxt").data(),               // Id
        xstrUtf16(mAndroidJson / "HcsSystem").data(),   // configuration
        hcsOperation.get(),                             // Operation
        nullptr,                                        // reserved
        &mHcsSystem                                     // ComputeSystem
    );
    std::cout << std::format("{} - HcsCreateComputeSystem:\nresult {}", __func__, result) << "\n";

    wil::unique_cotaskmem_string errStr;
    result = VmmgrHypervApi::HcsWaitForOperationResult(hcsOperation.get(), INFINITE, &errStr);
    std::cout << std::format("{} - HcsWaitForOperationResult:\nresult {}\nerrStr {}", __func__, result, xstrUtf8(errStr.get())) << "\n";
}

void configureHdvHost()
{
    HRESULT result = VmmgrHypervApi::HdvInitializeDeviceHost(mHcsSystem.get(), &mHdvHost);
    std::cout << std::format("{} - HdvInitializeDeviceHost\nresult {}", __func__, result) << "\n";
}

int main()
{
    std::string path;
    std::cout << "Enter the path of hypervm.json (without quotes): ";
    std::getline(std::cin, path);

    if (std::filesystem::exists(std::filesystem::path(path)))
    {
        std::cout << "----Execution started----\n";
        
        mAndroidJson = xjsonReadFromFile(std::filesystem::path(path));
        VmmgrHypervApi::init();
        
        configureHcnNetwork();
        configureHcnEndpoint();
        configureHcsSystem();
        configureHdvHost();
        
        std::cout << "----Execution finished----\n";
    }
    else
    {
        std::cout << "----No such file exists----\n";
    }

    return 0;
}