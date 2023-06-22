#include <iostream>
#include <format>
#include <filesystem>
#include <fstream>

#include <boost/json.hpp>

#include <Windows.h>
#include <wil/resource.h>
#include <ComputeNetwork.h>

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
}

boost::json::value mAndroidJson;
wil::unique_any < HCN_NETWORK, decltype(&::HcnCloseNetwork), [](HCN_NETWORK h) { return VmmgrHypervApi::HcnCloseNetwork(h); } > mHcnNetwork;
wil::unique_any < HCN_ENDPOINT, decltype(&::HcnCloseEndpoint), [](HCN_ENDPOINT h) { return VmmgrHypervApi::HcnCloseEndpoint(h); } > mHcnEndpoint;

void configureHcnNetwork()
{
    std::string networkGuid = (mAndroidJson / "HcnNetwork" / "ID").as_string().data();
    GUID guidNetwork;

    if (UuidFromStringA((RPC_CSTR)networkGuid.data(), &guidNetwork) != RPC_S_OK)
    {
        std::cout << std::format("{} - Failed to parse Network guid: {}\n", __func__, networkGuid) << "\n";
    }

    wil::unique_cotaskmem_string errStr;
    HRESULT result = VmmgrHypervApi::HcnOpenNetwork(guidNetwork, &mHcnNetwork, &errStr);

    std::cout << std::format("{} - HcnOpenNetwork:\nresult {}\nerrStr {}\n", __func__, result, xstrUtf8(errStr.get())) << "\n";

    if (result == HCN_E_NETWORK_NOT_FOUND)
    {
        result = VmmgrHypervApi::HcnCreateNetwork(
            guidNetwork,                                    // Id
            xstrUtf16(mAndroidJson / "HcnNetwork").data(),  // Settings
            &mHcnNetwork,                                   // Network
            &errStr                                         // ErrorRecord
        );

        std::cout << std::format("{} - HcnCreateNetwork\nresult {}\nerrStr {}\n", __func__, result, xstrUtf8(errStr.get())) << "\n";
    }
}

void configureHcnEndpoint()
{
    std::string endpointGuid = (mAndroidJson / "HcsSystem" / "VirtualMachine" / "Devices" /
        "NetworkAdapters" / "default" / "EndpointId").as_string().data();

    GUID guidEndpoint;
    if (UuidFromStringA((RPC_CSTR)endpointGuid.data(), &guidEndpoint) != RPC_S_OK)
    {
        std::cout << std::format("{} - Failed to parse Endpoint guid: {}\n", __func__, endpointGuid);
    }

    wil::unique_cotaskmem_string errStr;
    HRESULT result = VmmgrHypervApi::HcnDeleteEndpoint(guidEndpoint, &errStr);
    std::cout << std::format("{} - HcnDeleteEndpoint:\nresult {}\nerrStr {}\n", __func__, result, xstrUtf8(errStr.get())) << "\n";

    result = VmmgrHypervApi::HcnCreateEndpoint(
        mHcnNetwork.get(),                                  // Network
        guidEndpoint,                                       // Id
        xstrUtf16(mAndroidJson / "HcnEndpoint").data(),     // Settings
        &mHcnEndpoint,                                      // Endpoint
        &errStr);                                           // ErrorRecord

    std::cout << std::format("{} - HcnCreateEndpoint\nresult {}\nerrStr {}\n", __func__, result, xstrUtf8(errStr.get())) << "\n";
}

int main()
{
    std::string userName = getenv("USERNAME");
    std::string groupName = "Hyper-V Administrators";

    std::string command = std::format("net localgroup \"{}\" /add \"{}\"", groupName, userName);
    system(command.c_str());

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

        std::cout << "----Execution finished----\n";
    }
    else
    {
        std::cout << "----No such file exists----\n";
        return -1;
    }

    return 0;
}
