/**
 * Appcelerator Titanium Mobile
 * Copyright (c) 2017 by Appcelerator, Inc. All Rights Reserved.
 * Licensed under the terms of the Apache Public License
 * Please see the LICENSE included with this distribution for details.
 */
#include <cassert> // for assert
#include <vector> // std::vector
#include <v8-platform.h>
#include <libplatform/libplatform.h> // to pump message loop
#include "InspectorClient.h"
#include "InspectorFrontend.h" // new InspectorFrontend
#include "V8Util.h" // titanium::TwoByteValue and DEFINE_METHOD
#include "V8Runtime.h" // V8Runtime::v8_isolate
#include "JSDebugger.h" // JSDebugger::WaitForMessage()

#define TAG "InspectorClient"

namespace titanium {

bindings::BindEntry InspectorClient::bind_entry = {"inspector", Initialize, NULL};

InspectorClient::InspectorClient(v8::Local<v8::Context> context, v8::Platform* platform)
	: platform_(platform)
	, terminated_(false)
	, running_nested_loop_(false)
{
	// FIXME Replace reference to V8Runtime::v8_isolate with isolate_
	isolate_ = V8Runtime::v8_isolate;
	inspector_ = v8_inspector::V8Inspector::create(V8Runtime::v8_isolate, this);
	titanium::TwoByteValue contextName(STRING_NEW(V8Runtime::v8_isolate, "Titanium Main Context"));
	inspector_->contextCreated(v8_inspector::V8ContextInfo(
			context, kContextGroupId, v8_inspector::StringView(*contextName, contextName.length())));

	// Place a binding for 'inspector' into KrollBindings and have it expose the callAndPauseOnStart method to JS
	KrollBindings::addExternalBinding("inspector", &InspectorClient::bind_entry);
}

void InspectorClient::disconnect()
{
	v8::HandleScope scope(V8Runtime::v8_isolate);
	v8::Local<v8::Context> context = V8Runtime::GlobalContext();
	channel_.reset(nullptr);
	session_.reset(nullptr);
	inspector_.reset(nullptr);
}

void InspectorClient::connect()
{
	v8::HandleScope scope(V8Runtime::v8_isolate);
	v8::Local<v8::Context> context = V8Runtime::GlobalContext();
	channel_.reset(new InspectorFrontend(context));
	session_ = inspector_->connect(1, channel_.get(), v8_inspector::StringView());
}

void InspectorClient::BreakAtStart()
{
	v8::HandleScope scope(V8Runtime::v8_isolate);
	titanium::TwoByteValue pauseReason(STRING_NEW(V8Runtime::v8_isolate, "PauseOnNextStatement"));
	session_->schedulePauseOnNextStatement(v8_inspector::StringView(*pauseReason, pauseReason.length()), v8_inspector::StringView());
}

void InspectorClient::sendMessage(const v8_inspector::StringView& message_view)
{
	assert(session_ != nullptr);

	session_->dispatchProtocolMessage(message_view);
}

void InspectorClient::runMessageLoopOnPause(int context_group_id)
{
	assert(channel_ != nullptr);
	if (running_nested_loop_) {
		return;
	}

	terminated_ = false;
	running_nested_loop_ = true;
	while (!terminated_) {
		v8::Local<v8::String> message = JSDebugger::WaitForMessage();
		titanium::TwoByteValue buffer(message);
		v8_inspector::StringView message_view(*buffer, buffer.length());
		sendMessage(message_view);

		while (v8::platform::PumpMessageLoop(platform_, V8Runtime::v8_isolate)) {}
	}

	terminated_ = false;
	running_nested_loop_ = false;
}

void InspectorClient::quitMessageLoopOnPause()
{
	terminated_ = true;
}

void InspectorClient::Initialize(v8::Local<v8::Object> target, v8::Local<v8::Context> context)
{
	v8::Isolate* isolate = context->GetIsolate();
	v8::HandleScope scope(isolate);

	DEFINE_METHOD(isolate, target, "callAndPauseOnStart", CallAndPauseOnStart);
}

void InspectorClient::CallAndPauseOnStart(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	v8::Isolate* isolate = args.GetIsolate();
	v8::HandleScope scope(isolate);

	assert(args.Length() >= 1);
	assert(args[0]->IsFunction());

	// copy args
	std::vector<v8::Local<v8::Value>> call_args;
	for (int i = 2; i < args.Length(); i++) {
		call_args.push_back(args[i]);
	}

	// Calls #BreakAtStart() on inspector client instance, which calls schedulePauseOnNextStatement
	// This basically queues up a pause to happen as soon as we invoke app.js
	JSDebugger::debugBreak();

	// Now invoke the function wrapping app.js
	v8::TryCatch tryCatch(isolate);
	v8::Local<v8::Value> result;
	v8::MaybeLocal<v8::Value> retval =
			args[0].As<v8::Function>()->Call(isolate->GetCurrentContext(), args[1],
																			 call_args.size(), call_args.data());
	if (!retval.ToLocal(&result)) {
		titanium::V8Util::fatalException(isolate, tryCatch);
		return;
	}
	args.GetReturnValue().Set(result);
}

} // namespace titanium
