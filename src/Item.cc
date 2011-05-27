#include "Item.h"

using namespace node;
using namespace v8;

namespace node_iTunes {

static Persistent<String> ITEM_CLASS_SYMBOL;

//Persistent<FunctionTemplate> Item::constructor_template;

void Item::Init(v8::Handle<Object> target) {
  HandleScope scope;

  // String Constants
  ITEM_CLASS_SYMBOL = NODE_PSYMBOL("Item");

  // Set up the 'Item' base-class constructor template
  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  item_constructor_template = Persistent<FunctionTemplate>::New(t);
  item_constructor_template->SetClassName(ITEM_CLASS_SYMBOL);
  t->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(t, "getNameSync", GetNameSync);
  //NODE_SET_METHOD(target, "createConnection", CreateConnection);

  target->Set(ITEM_CLASS_SYMBOL, item_constructor_template->GetFunction());
}


Item::Item() {
}

Item::~Item() {
}

v8::Handle<Value> Item::New(const Arguments& args) {
  HandleScope scope;
  //if (args.Length() != 1 || !NEW_CHECKER->StrictEquals(args[0]->ToObject())) {
  //  return ThrowException(Exception::TypeError(String::New("Use '.createConnection()' to get an Application instance")));
  //}

  Item* item = new Item();
  item->Wrap(args.This());
  return args.This();
}

v8::Handle<Value> Item::GetNameSync(const Arguments& args) {
  HandleScope scope;
  Item* it = ObjectWrap::Unwrap<Item>(args.This());
  iTunesItem* item = it->itemRef;
  Local<Value> result = String::New([[item name] UTF8String]);
  return scope.Close(result);
}

/*v8::Handle<Value> Application::QuitSync(const Arguments& args) {
  HandleScope scope;
  Application* it = ObjectWrap::Unwrap<Application>(args.This());
  iTunesApplication* iTunes = it->iTunesRef;
  [iTunes quit];
  return Undefined();
}

v8::Handle<Value> Application::GetVolumeSync(const Arguments& args) {
  HandleScope scope;
  Application* it = ObjectWrap::Unwrap<Application>(args.This());
  iTunesApplication* iTunes = it->iTunesRef;
  Local<Integer> result = Integer::New([iTunes soundVolume]);
  return scope.Close(result);
}

v8::Handle<Value> Application::SetVolumeSync(const Arguments& args) {
  HandleScope scope;
  Application* it = ObjectWrap::Unwrap<Application>(args.This());
  iTunesApplication* iTunes = it->iTunesRef;
  int val = args[0]->ToInteger()->Int32Value();
  [iTunes setSoundVolume:val];
  Local<Integer> result = Integer::New([iTunes soundVolume]);
  return scope.Close(result);
}

// Begins asynchronously creating a new Application instance. This should be
// done on the thread pool, since the SBApplication constructor methods can
// potentially block for a very long time (especially if a modal dialog is
// raised for user credentials).
v8::Handle<Value> Application::CreateConnection(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1) {
    return ThrowException(Exception::TypeError(String::New("A callback function is required")));
  }

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

  eio_custom(CreateConnection_Do, EIO_PRI_DEFAULT, CreateConnection_After, ccr);
  ev_ref(EV_DEFAULT_UC);
  return Undefined();
}

// Creates the SBApplication instance. This is called on the thread pool since
// it can block for a long time.
int CreateConnection_Do (eio_req *req) {
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

int CreateConnection_After (eio_req *req) {
  HandleScope scope;
  ev_unref(EV_DEFAULT_UC);
  struct create_connection_request * ccr = (struct create_connection_request *)req->data;
  Local<Value> argv[2];
  argv[0] = Local<Value>::New(Null());

  // We need to create an instance of the JS 'Application' class
  v8::Handle<Value> constructor_args[1];
  constructor_args[0] = NEW_CHECKER;
  Local<Object> app = Application::constructor_template->GetFunction()->NewInstance(1, constructor_args);
  Application* it = ObjectWrap::Unwrap<Application>(app);
  it->iTunesRef = ccr->iTunesRef;
  argv[1] = app;

  //argv[2] = String::New(sr->name);
  TryCatch try_catch;
  ccr->cb->Call(Context::GetCurrent()->Global(), 2, argv);
  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
  ccr->cb.Dispose();
  free(ccr);
  return 0;
}*/

} // namespace node_iTunes
