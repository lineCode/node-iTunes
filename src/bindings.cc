#import <v8.h>
#import <node.h>

#import "Application.h"
#import "Item.h"
#import "Track.h"
#import "URLTrack.h"
#import "iTunes.h"

using namespace node;
using namespace v8;

namespace node_iTunes {

v8::Persistent<v8::FunctionTemplate> application_constructor_template;
v8::Persistent<v8::FunctionTemplate> item_constructor_template;
v8::Persistent<v8::FunctionTemplate> track_constructor_template;
v8::Persistent<v8::FunctionTemplate> url_track_constructor_template;

extern "C" void init(v8::Handle<Object> target) {
  HandleScope scope;
  Application::Init(target);
  Item::Init(target);
  Track::Init(target);
  URLTrack::Init(target);
}

} // namespace node_iTunes
