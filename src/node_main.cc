// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "node.h"
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <VersionHelpers.h>
#include <WinError.h>

#define SKIP_CHECK_VAR "NODE_SKIP_PLATFORM_CHECK"
#define SKIP_CHECK_SIZE 1
#define SKIP_CHECK_VALUE "1"


// https://github.com/nodejs/node/blob/v14.21.1/test/embedding/embedtest.cc
#include "node.h"
#include "uv.h"
#include <assert.h>

using node::ArrayBufferAllocator;
using node::Environment;
using node::IsolateData;
using node::MultiIsolatePlatform;
using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Locker;
using v8::MaybeLocal;
using v8::SealHandleScope;
using v8::V8;
using v8::Value;

static int RunNodeInstance(MultiIsolatePlatform* platform,
                           const std::vector<std::string>& args,
                           const std::vector<std::string>& exec_args);

int mmain(int argc, char** argv) {
  argv = uv_setup_args(argc, argv);
  std::vector<std::string> args(argv, argv + argc);
  std::vector<std::string> exec_args;
  std::vector<std::string> errors;
  int exit_code = node::InitializeNodeWithArgs(&args, &exec_args, &errors);
  for (const std::string& error : errors)
    fprintf(stderr, "%s: %s\n", args[0].c_str(), error.c_str());
  if (exit_code != 0) {
    return exit_code;
  }

  std::unique_ptr<MultiIsolatePlatform> platform =
      MultiIsolatePlatform::Create(4);
  V8::InitializePlatform(platform.get());
  V8::Initialize();

  int ret = RunNodeInstance(platform.get(), args, exec_args);

  V8::Dispose();
  V8::ShutdownPlatform();
  return ret;
}

int RunNodeInstance(MultiIsolatePlatform* platform,
                    const std::vector<std::string>& args,
                    const std::vector<std::string>& exec_args) {
  int exit_code = 0;
  uv_loop_t loop;
  int ret = uv_loop_init(&loop);
  if (ret != 0) {
    fprintf(stderr,
            "%s: Failed to initialize loop: %s\n",
            args[0].c_str(),
            uv_err_name(ret));
    return 1;
  }

  std::shared_ptr<ArrayBufferAllocator> allocator =
      ArrayBufferAllocator::Create();

  Isolate* isolate = NewIsolate(allocator, &loop, platform);
  if (isolate == nullptr) {
    fprintf(stderr, "%s: Failed to initialize V8 Isolate\n", args[0].c_str());
    return 1;
  }

  {
    Locker locker(isolate);
    Isolate::Scope isolate_scope(isolate);

    std::unique_ptr<IsolateData, decltype(&node::FreeIsolateData)> isolate_data(
        node::CreateIsolateData(isolate, &loop, platform, allocator.get()),
        node::FreeIsolateData);

    HandleScope handle_scope(isolate);
    Local<Context> context = node::NewContext(isolate);
    if (context.IsEmpty()) {
      fprintf(stderr, "%s: Failed to initialize V8 Context\n", args[0].c_str());
      return 1;
    }

    Context::Scope context_scope(context);
    std::unique_ptr<Environment, decltype(&node::FreeEnvironment)> env(
        node::CreateEnvironment(isolate_data.get(), context, args, exec_args),
        node::FreeEnvironment);

    MaybeLocal<Value> loadenv_ret = node::LoadEnvironment(
        env.get(),
        "const publicRequire ="
        "  require('module').createRequire(process.cwd() + '/');"
        "globalThis.require = publicRequire;"
        "globalThis.embedVars = { no: require('fs') };"
        "import('fs');"
        "require('vm').runInThisContext(`console.log(embedVars)`);");

    if (loadenv_ret.IsEmpty())  // There has been a JS exception.
      return 1;

    {
      SealHandleScope seal(isolate);
      bool more;
      do {
        uv_run(&loop, UV_RUN_DEFAULT);

        platform->DrainTasks(isolate);
        more = uv_loop_alive(&loop);
        if (more) continue;

        if (node::EmitProcessBeforeExit(env.get()).IsNothing()) break;

        more = uv_loop_alive(&loop);
      } while (more == true);
    }

    exit_code = node::EmitProcessExit(env.get()).FromMaybe(1);

    node::Stop(env.get());
  }

  bool platform_finished = false;
  platform->AddIsolateFinishedCallback(
      isolate,
      [](void* data) { *static_cast<bool*>(data) = true; },
      &platform_finished);
  platform->UnregisterIsolate(isolate);
  isolate->Dispose();

  // Wait until the platform has cleaned up all relevant resources.
  while (!platform_finished) uv_run(&loop, UV_RUN_ONCE);
  int err = uv_loop_close(&loop);
  assert(err == 0);

  return exit_code;
}

// 真入口点，nede.exe node.dll 都调用此函数
extern "C" __declspec(dllexport) int wmain(int argc, wchar_t* wargv[]) {

    char* argv2[] = {
      "C:\\projects\\edge-js\\tools\\build\\node-14.21.1\\out\\Debug\\node2."
      "exe",
      //(wchar_t*)lpCmdLine,
      // L"C:\\projects\\edge-js\\tools\\build\\node-14.21.1\\out\\Debug\\pmserver\\server.js",
      "D:\\GitHub\\echodict\\pmserver\\server.js"};

     //mmain(argc, argv2);

   //argc = 2;

   //wchar_t* wargv2[] = {
   //   L"C:\\projects\\edge-js\\tools\\build\\node-14.21.1\\out\\Debug\\node2.exe",
   //   //(wchar_t*)lpCmdLine,
   //   // L"C:\\projects\\edge-js\\tools\\build\\node-14.21.1\\out\\Debug\\pmserver\\server.js",
   //   L"D:\\GitHub\\echodict\\pmserver\\server.js"
   //};

   //wargv = wargv2;

  // Windows Server 2012 (not R2) is supported until 10/10/2023, so we allow it
  // to run in the experimental support tier.
  char buf[SKIP_CHECK_SIZE + 1];
  if (!IsWindows8Point1OrGreater() &&
      !(IsWindowsServer() && IsWindows8OrGreater()) &&
      (GetEnvironmentVariableA(SKIP_CHECK_VAR, buf, sizeof(buf)) !=
       SKIP_CHECK_SIZE ||
       strncmp(buf, SKIP_CHECK_VALUE, SKIP_CHECK_SIZE + 1) != 0)) {
    //fprintf(stderr, "Node.js is only supported on Windows 8.1, Windows "
    //                "Server 2012 R2, or higher.\n"
    //                "Setting the " SKIP_CHECK_VAR " environment variable "
    //                "to 1 skips this\ncheck, but Node.js might not execute "
    //                "correctly. Any issues encountered on\nunsupported "
    //                "platforms will not be fixed.");
    //exit(ERROR_EXE_MACHINE_TYPE_MISMATCH);
  }

  // Convert argv to UTF8
  char** argv = new char*[argc + 1];
  //char *argv = new char[argc + 1];
  for (int i = 0; i < argc; i++) {
    // Compute the size of the required buffer
    DWORD size = WideCharToMultiByte(CP_UTF8,
                                     0,
                                     wargv[i],
                                     -1,
                                     nullptr,
                                     0,
                                     nullptr,
                                     nullptr);
    if (size == 0) {
      // This should never happen.
      fprintf(stderr, "Could not convert arguments to utf8.");
      exit(1);
    }
    // Do the actual conversion
    argv[i] = new char[size];
    DWORD result = WideCharToMultiByte(CP_UTF8,
                                       0,
                                       wargv[i],
                                       -1,
                                       argv[i],
                                       size,
                                       nullptr,
                                       nullptr);
    if (result == 0) {
      // This should never happen.
      fprintf(stderr, "Could not convert arguments to utf8.");
      exit(1);
    }
  }
  argv[argc] = nullptr;

  // Now that conversion is done, we can finally start.

  return node::Start(argc, argv);
}
#else
// UNIX
#ifdef __linux__
#include <elf.h>
#ifdef __LP64__
#define Elf_auxv_t Elf64_auxv_t
#else
#define Elf_auxv_t Elf32_auxv_t
#endif  // __LP64__
extern char** environ;
#endif  // __linux__
#if defined(__POSIX__) && defined(NODE_SHARED_MODE)
#include <string.h>
#include <signal.h>
#endif

namespace node {
namespace per_process {
extern bool linux_at_secure;
}  // namespace per_process
}  // namespace node

int main(int argc, char* argv[]) {
#if defined(__POSIX__) && defined(NODE_SHARED_MODE)
  // In node::PlatformInit(), we squash all signal handlers for non-shared lib
  // build. In order to run test cases against shared lib build, we also need
  // to do the same thing for shared lib build here, but only for SIGPIPE for
  // now. If node::PlatformInit() is moved to here, then this section could be
  // removed.
  {
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, nullptr);
  }
#endif

#if defined(__linux__)
  char** envp = environ;
  while (*envp++ != nullptr) {}
  Elf_auxv_t* auxv = reinterpret_cast<Elf_auxv_t*>(envp);
  for (; auxv->a_type != AT_NULL; auxv++) {
    if (auxv->a_type == AT_SECURE) {
      node::per_process::linux_at_secure = auxv->a_un.a_val;
      break;
    }
  }
#endif
  // Disable stdio buffering, it interacts poorly with printf()
  // calls elsewhere in the program (e.g., any logging from V8.)
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
  return node::Start(argc, argv);
}
#endif

#include <string>
int WinMain(HINSTANCE hInstance,
            HINSTANCE hPrevInstance,
            LPSTR lpCmdLine,
            int nShowCmd) {

  int argc = 1;

  wchar_t* wargv[] = {
    L"C:\\projects\\edge-js\\tools\\build\\node-14.21.1\\out\\Debug\\node2.exe",
    //(wchar_t*)lpCmdLine,
    //L"C:\\projects\\edge-js\\tools\\build\\node-14.21.1\\out\\Debug\\pmserver\\server.js",
    L"D:\\GitHub\\echodict\\pmserver\\server.js"
  };

  char * argv[] = {
    "C:\\projects\\edge-js\\tools\\build\\node-14.21.1\\out\\Debug\\node2.exe",
    //(wchar_t*)lpCmdLine,
    //L"C:\\projects\\edge-js\\tools\\build\\node-14.21.1\\out\\Debug\\pmserver\\server.js",
    "D:\\GitHub\\echodict\\pmserver\\server.js"
  };
  //mmain(argc, argv);

  //char** argv = new char*[argc + 1];

  //wchar_t *v1 =  // 这参数是没有用到的

  //wchar_t *v2 = L"C:\\projects\\edge-js\\tools\\build\\node-14.21.1\\out\\Debug\\pmserver\\server.js";  // 脚本路径


  //// 先算大小
  //DWORD size = WideCharToMultiByte(
  //    CP_UTF8, 0, v1, -1, nullptr, 0, nullptr, nullptr);
  //DWORD size2 = WideCharToMultiByte(
  //    CP_UTF8, 0, v2, -1, nullptr, 0, nullptr, nullptr);

  //// 再转 utf8
  //argv[0] = new char[size];
  //DWORD result = WideCharToMultiByte(
  //    CP_UTF8, 0, v1, -1, argv[0], size, nullptr, nullptr);
  //argv[1] = new char[size2];
  //DWORD result = WideCharToMultiByte(
  //    CP_UTF8, 0, v2, -1, argv[1], size2, nullptr, nullptr);


  wmain(argc, wargv);

  return 0;
}
