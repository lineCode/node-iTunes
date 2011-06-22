#include "Application.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace node;
using namespace v8;

namespace node_iTunes {

static Persistent<String> APPLICATION_CLASS_SYMBOL;
static Persistent<String> HOST_SYMBOL;
static Persistent<String> USERNAME_SYMBOL;
static Persistent<String> PASSWORD_SYMBOL;

// Stupid little object used to ensure that users aren't calling 'new
// Application' but rather 'iTunes.createConnection()'. There's probably
// a better way to do this...
static Persistent<Object> NEW_CHECKER;

struct create_connection_request {
  char* host;
  char* username;
  char* password;
  v8::Persistent<v8::Function> cb;
  iTunesApplication* iTunesRef;
};

struct async_request {
  Persistent<Function> callback;
  Persistent<Object> thisRef;
  iTunesApplication *iTunesRef;
  void *result;
  char *id;
  pthread_mutex_t *mutex;
};

//Persistent<FunctionTemplate> Application::constructor_template;

void Application::Init(v8::Handle<Object> target) {
  HandleScope scope;

  // String Constants
  APPLICATION_CLASS_SYMBOL = NODE_PSYMBOL("Application");
  HOST_SYMBOL = NODE_PSYMBOL("host");
  USERNAME_SYMBOL = NODE_PSYMBOL("username");
  PASSWORD_SYMBOL = NODE_PSYMBOL("password");

  // This Object is used in the Application JS constructor to ensure that the
  // user isn't calling 'new Application', but rather '.createConnection()'.
  NEW_CHECKER = Persistent<Object>::New(Object::New());

  // Set up the 'Application' constructor template
  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  application_constructor_template = Persistent<FunctionTemplate>::New(t);
  application_constructor_template->SetClassName(APPLICATION_CLASS_SYMBOL);
  t->InstanceTemplate()->SetInternalFieldCount(1);

  //NODE_SET_PROTOTYPE_METHOD(t, "run", GetVolume);
  NODE_SET_PROTOTYPE_METHOD(t, "running", Running);
  NODE_SET_PROTOTYPE_METHOD(t, "quit", Quit);
  //NODE_SET_PROTOTYPE_METHOD(t, "add", GetVolume);
  //NODE_SET_PROTOTYPE_METHOD(t, "backTrack", GetVolume);
  //NODE_SET_PROTOTYPE_METHOD(t, "convert", GetVolume);
  //NODE_SET_PROTOTYPE_METHOD(t, "fastForward", GetVolume);
  //NODE_SET_PROTOTYPE_METHOD(t, "nextTrack", GetVolume);
  //NODE_SET_PROTOTYPE_METHOD(t, "pause", GetVolume);
  //NODE_SET_PROTOTYPE_METHOD(t, "playOnce", GetVolume);
  //NODE_SET_PROTOTYPE_METHOD(t, "playpause", GetVolume);
  //NODE_SET_PROTOTYPE_METHOD(t, "previousTrack", GetVolume);
  //NODE_SET_PROTOTYPE_METHOD(t, "resume", GetVolume);
  //NODE_SET_PROTOTYPE_METHOD(t, "rewind", GetVolume);
  //NODE_SET_PROTOTYPE_METHOD(t, "stop", GetVolume);
  //NODE_SET_PROTOTYPE_METHOD(t, "update", GetVolume);
  //NODE_SET_PROTOTYPE_METHOD(t, "eject", GetVolume);
  //NODE_SET_PROTOTYPE_METHOD(t, "subscribe", GetVolume);
  //NODE_SET_PROTOTYPE_METHOD(t, "updateAllPodcasts", GetVolume);
  //NODE_SET_PROTOTYPE_METHOD(t, "updatePodcast", GetVolume);
  //NODE_SET_PROTOTYPE_METHOD(t, "openLocation", GetVolume);
  NODE_SET_PROTOTYPE_METHOD(t, "currentTrack", CurrentTrack);
  NODE_SET_PROTOTYPE_METHOD(t, "selection", Selection);
  NODE_SET_PROTOTYPE_METHOD(t, "volume", Volume);

  NODE_SET_METHOD(target, "createConnection", CreateConnection);

  NODE_SET_PROTOTYPE_METHOD(t, "toString", ToString);

  target->Set(APPLICATION_CLASS_SYMBOL, application_constructor_template->GetFunction());
}


Application::Application() {
  int err = pthread_mutex_init(&mutex, NULL);
  if (err) {
    printf("Got Error! %d", err);
  }
}

Application::~Application() {
  if (iTunesRef) {
    [iTunesRef release];
  }
  iTunesRef = nil;
  int err = pthread_mutex_destroy(&mutex);
  if (err) {
    printf("Got Error! %d", err);
  }
}

v8::Handle<Value> Application::New(const Arguments& args) {
  HandleScope scope;
   if (args.Length() != 1 || !NEW_CHECKER->StrictEquals(args[0]->ToObject())) {
    return ThrowException(Exception::TypeError(String::New("Use '.createConnection()' to get an Application instance")));
  }

  Application* hw = new Application();
  hw->Wrap(args.This());
  return args.This();
}

// ToString //////////////////////////////////////////////////////////////////
v8::Handle<Value> Application::ToString(const Arguments& args) {
  HandleScope scope;
  Application *app = ObjectWrap::Unwrap<Application>(args.This());
  Local<String> str = String::New([[app->iTunesRef description] UTF8String]);
  return scope.Close(str);
}

v8::Handle<Value> Application::Running(const Arguments& args) {
  HandleScope scope;
  Application* it = ObjectWrap::Unwrap<Application>(args.This());
  iTunesApplication* iTunes = it->iTunesRef;
  v8::Handle<v8::Boolean> result = v8::Boolean::New([iTunes isRunning]);
  return scope.Close(result);
}

v8::Handle<Value> Application::Quit(const Arguments& args) {
  HandleScope scope;
  Application* it = ObjectWrap::Unwrap<Application>(args.This());
  iTunesApplication* iTunes = it->iTunesRef;
  [iTunes quit];
  return Undefined();
}

// CurrentTrack ///////////////////////////////////////////////////////////////
v8::Handle<Value> Application::CurrentTrack(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1) {
    return ThrowException(Exception::TypeError(String::New("A callback function is required")));
  }

  Application* it = ObjectWrap::Unwrap<Application>(args.This());

  async_request *ar = (async_request *)malloc(sizeof(struct async_request));
  ar->iTunesRef = it->iTunesRef;
  Local<Function> cb = Local<Function>::Cast(args[0]);
  ar->callback = Persistent<Function>::New(cb);
  ar->thisRef = Persistent<Object>::New(args.This());
  ar->mutex = &it->mutex;

  eio_custom(EIO_CurrentTrack, EIO_PRI_DEFAULT, EIO_AfterCurrentTrack, ar);
  ev_ref(EV_DEFAULT_UC);

  return scope.Close(Undefined());
}

int Application::EIO_CurrentTrack(eio_req *req) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  async_request *ar = (async_request *)req->data;
  pthread_mutex_lock( ar->mutex );
  usleep(10 * 1000);
  iTunesTrack *track = [[ar->iTunesRef currentTrack] get];
  [track retain];
  ar->result = (void *)track;
  ar->id = (char *)[[track persistentID] UTF8String];
  pthread_mutex_unlock( ar->mutex );
  [pool drain];
  return 0;
}

int Application::EIO_AfterCurrentTrack(eio_req *req) {
  HandleScope scope;
  ev_unref(EV_DEFAULT_UC);
  async_request *ar = (async_request *)req->data;

  TryCatch try_catch;
  v8::Handle<Value> argv[2];
  // TODO: Error Handling
  argv[0] = Null();
  argv[1] = Item::WrapInstance((iTunesItem *)ar->result, ar->id);
  ar->callback->Call(ar->thisRef, 2, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }

  ar->callback.Dispose();
  ar->thisRef.Dispose();
  free(ar);
  return 0;
}


// Selection //////////////////////////////////////////////////////////////////
v8::Handle<Value> Application::Selection(const Arguments& args) {
  HandleScope scope;
  //Application* it = ObjectWrap::Unwrap<Application>(args.This());
  //iTunesApplication* iTunes = it->iTunesRef;
  //SBElementArray* selection = [iTunes selection];
  //NSArray* selection = [[iTunes selection] get];
  //Local<Object> Track::WrapInstance
  return Undefined();
}

// Volume /////////////////////////////////////////////////////////////////////
v8::Handle<Value> Application::Volume(const Arguments& args) {
  HandleScope scope;

  Application* it = ObjectWrap::Unwrap<Application>(args.This());

  async_request *ar = (async_request *)malloc(sizeof(struct async_request));
  ar->iTunesRef = it->iTunesRef;
  Local<Function> cb = Local<Function>::Cast(args[0]);
  ar->callback = Persistent<Function>::New(cb);
  ar->thisRef = Persistent<Object>::New(args.This());

  eio_custom(EIO_Volume, EIO_PRI_DEFAULT, EIO_AfterVolume, ar);
  ev_ref(EV_DEFAULT_UC);
  return scope.Close(Undefined());
}

int Application::EIO_Volume(eio_req *req) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  async_request *ar = (async_request *)req->data;
  NSInteger vol = [ar->iTunesRef soundVolume];
  req->result = vol;
  [pool drain];
  return 0;
}

int Application::EIO_AfterVolume(eio_req *req) {
  HandleScope scope;
  ev_unref(EV_DEFAULT_UC);
  async_request *ar = (async_request *)req->data;

  TryCatch try_catch;
  v8::Handle<Value> argv[2];
  // TODO: Error Handling
  argv[0] = Null();
  argv[1] = Integer::New(req->result);
  ar->callback->Call(ar->thisRef, 2, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }

  ar->callback.Dispose();
  ar->thisRef.Dispose();
  free(ar);
  return 0;
}

// Begins asynchronously creating a new Application instance. This should be
// done on the thread pool, since the SBApplication constructor methods can
// potentially block for a very long time (especially if a modal dialog is
// presented for user credentials).
v8::Handle<Value> Application::CreateConnection(const Arguments& args) {
  HandleScope scope;

  create_connection_request* ccr = (create_connection_request *) malloc(sizeof(struct create_connection_request));
  ccr->host = NULL;
  ccr->username = NULL;
  ccr->password = NULL;

  int cbIndex = 0;
  if (args[1]->IsFunction()) {
    cbIndex = 1;
    Local<Object> options = args[0]->ToObject();
    if (options->Has(HOST_SYMBOL)) {
      String::Utf8Value hostValue(options->Get(HOST_SYMBOL));
      char* hostStr = (char*)malloc(strlen(*hostValue) + 1);
      strcpy(hostStr, *hostValue);
      ccr->host = hostStr;
    } else {
      return ThrowException(Exception::TypeError(String::New("The 'host' property is required when an 'options' Object is given")));
    }
    if (options->Has(USERNAME_SYMBOL)) {
      String::Utf8Value unValue(options->Get(USERNAME_SYMBOL));
      char* unStr = (char*)malloc(strlen(*unValue) + 1);
      strcpy(unStr, *unValue);
      ccr->username = unStr;
    }
    if (options->Has(PASSWORD_SYMBOL)) {
      String::Utf8Value pwValue(options->Get(PASSWORD_SYMBOL));
      char* pwStr = (char*)malloc(strlen(*pwValue) + 1);
      strcpy(pwStr, *pwValue);
      ccr->password = pwStr;
    }
  }
  Local<Function> cb = Local<Function>::Cast(args[cbIndex]);
  ccr->cb = Persistent<Function>::New(cb);

  eio_custom(EIO_CreateConnection, EIO_PRI_DEFAULT, EIO_AfterCreateConnection, ccr);
  ev_ref(EV_DEFAULT_UC);
  return Undefined();
}

// Creates the SBApplication instance. This is called on the thread pool since
// it can block for a long time.
int Application::EIO_CreateConnection (eio_req *req) {
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  struct create_connection_request * ccr = (struct create_connection_request *)req->data;
  if (ccr->host != NULL) {
    //printf("host: %s\n", ccr->host);
    NSString* urlStr = @"eppc://";
    if (ccr->username != NULL && ccr->password != NULL) {
      //printf("username: %s\n", ccr->username);
      //printf("password: %s\n", ccr->password);
      NSString* unString = [NSString stringWithCString: ((const char*)ccr->username) encoding: NSUTF8StringEncoding ];
      urlStr = [urlStr stringByAppendingString: [unString stringByAddingPercentEscapesUsingEncoding: NSASCIIStringEncoding ]];
      urlStr = [urlStr stringByAppendingString: @":" ];
      NSString* pwString = [NSString stringWithCString: ((const char*)ccr->password) encoding: NSUTF8StringEncoding ];
      urlStr = [urlStr stringByAppendingString: [pwString stringByAddingPercentEscapesUsingEncoding: NSASCIIStringEncoding ]];
      urlStr = [urlStr stringByAppendingString: @"@" ];
      free(ccr->password);
      free(ccr->username);
    }
    urlStr = [urlStr stringByAppendingString: [NSString stringWithCString: ((const char*)ccr->host) encoding: NSUTF8StringEncoding ]];
    urlStr = [urlStr stringByAppendingString: @"/iTunes" ];
    //NSLog(@"%@", urlStr);
    NSURL* url = [NSURL URLWithString: urlStr];
    ccr->iTunesRef = [SBApplication applicationWithURL: url];
    free(ccr->host);
  } else {
    // If no 'options' object was provided, then simply connect to the local
    // iTunes installation. No credentials are required for this mode.
    ccr->iTunesRef = [SBApplication applicationWithBundleIdentifier:@"com.apple.iTunes"];
  }
  [pool drain];
  return 0;
}

int Application::EIO_AfterCreateConnection (eio_req *req) {
  HandleScope scope;
  ev_unref(EV_DEFAULT_UC);
  struct create_connection_request * ccr = (struct create_connection_request *)req->data;
  Local<Value> argv[2];
  argv[0] = Local<Value>::New(Null());

  // We need to create an instance of the JS 'Application' class
  v8::Handle<Value> constructor_args[1];
  constructor_args[0] = NEW_CHECKER;
  Local<Object> app = application_constructor_template->GetFunction()->NewInstance(1, constructor_args);
  Application* it = ObjectWrap::Unwrap<Application>(app);
  it->iTunesRef = ccr->iTunesRef;
  argv[1] = app;

  TryCatch try_catch;
  ccr->cb->Call(Context::GetCurrent()->Global(), 2, argv);
  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
  ccr->cb.Dispose();
  free(ccr);
  return 0;
}

} // namespace node_iTunes
