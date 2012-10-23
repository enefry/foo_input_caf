#include <memory>
#include "util.h"
#include "../SDK/foobar2000.h"

std::string g_CoreAudioToolboxVersion;
static HMODULE g_CoreAudioToolboxModule;

static
std::wstring getAppleApplicationSupportPath()
{
    HKEY hKey;
    const wchar_t *subkey =
	L"SOFTWARE\\Apple Inc.\\Apple Application Support";
    if (SUCCEEDED(RegOpenKeyExW(HKEY_LOCAL_MACHINE, subkey, 0,
				KEY_READ, &hKey))) {
	std::shared_ptr<HKEY__> hKeyPtr(hKey, RegCloseKey);
	DWORD size;
	if (SUCCEEDED(RegQueryValueExW(hKey, L"InstallDir", 0, 0, 0, &size))) {
	    std::vector<wchar_t> vec(size/sizeof(wchar_t));
	    if (SUCCEEDED(RegQueryValueExW(hKey, L"InstallDir", 0, 0,
			reinterpret_cast<LPBYTE>(&vec[0]), &size)))
		return &vec[0];
	}
    }
    return L"";
}

static
std::string getCoreAudioToolboxVersion(HMODULE hDll)
{
    HRSRC hRes = FindResourceExW(hDll,
			         RT_VERSION,
			         MAKEINTRESOURCEW(VS_VERSION_INFO),
				 MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
    std::string data;
    {
	DWORD cbres = SizeofResource(hDll, hRes);
	HGLOBAL hMem = LoadResource(hDll, hRes);
	if (hMem) {
	    char *pc = static_cast<char*>(LockResource(hMem));
	    if (pc && cbres)
		data.assign(pc, cbres);
	    FreeResource(hMem);
	}
    }
    // find dwSignature of VS_FIXEDFILEINFO
    std::stringstream ss;
    size_t pos = data.find("\xbd\x04\xef\xfe");
    if (pos != std::string::npos) {
	VS_FIXEDFILEINFO vfi;
	std::memcpy(&vfi, data.c_str() + pos, sizeof vfi);
	WORD v[4];
	v[0] = HIWORD(vfi.dwFileVersionMS);
	v[1] = LOWORD(vfi.dwFileVersionMS);
	v[2] = HIWORD(vfi.dwFileVersionLS);
	v[3] = LOWORD(vfi.dwFileVersionLS);
	ss << v[0] << "." << v[1] << "." << v[2] << "." << v[3];
    }
    return ss.str();
}

class init_input_caf: public initquit {
public:
    void on_init()
    {
	HMODULE self = reinterpret_cast<HMODULE>(core_api::get_my_instance());
	std::wstring path = util::get_module_directory(self);
	std::vector<std::wstring> candidates;
	candidates.push_back(path);
	candidates.push_back(path + L"QTfiles\\");
	if ((path = getAppleApplicationSupportPath()) != L"") {
	    if (path.back() != L'\\')
		path.push_back(L'\\');
	    candidates.push_back(path);
	}
	std::vector<std::wstring>::const_iterator it;
	HMODULE hmod = 0;
	for (it = candidates.begin(); it != candidates.end(); ++it) {
	    path = *it + L"CoreAudioToolbox.dll";
	    hmod = LoadLibraryExW(path.c_str(), 0,
				  LOAD_WITH_ALTERED_SEARCH_PATH);
	    if (hmod)
		break;
	}
	if (!hmod) {
	    console::error("foo_input_caf: can't load CoreAudioToolbox.dll");
	} else {
	    g_CoreAudioToolboxModule = hmod;
	    std::string version = getCoreAudioToolboxVersion(hmod);
	    g_CoreAudioToolboxVersion.swap(version);
	    std::string u8path = strutil::w2m(path, utf8_codecvt_facet());
	    console::formatter() << "foo_input_caf: "
				 << u8path.c_str()
				 << " version "
				 << g_CoreAudioToolboxVersion.c_str();
	}
    }
    void on_quit()
    {
	if (g_CoreAudioToolboxModule)
	    FreeLibrary(g_CoreAudioToolboxModule);
    }
};

static initquit_factory_t<init_input_caf> g_init_input_caf_factory;