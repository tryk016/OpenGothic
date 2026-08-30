#include "systemmsg.h"

#include <Tempest/Platform>

#if defined(__IOS__)

#import <UIKit/UIKit.h>

#include <cstdint>

static NSString* pendingTitle   = nil;
static NSString* pendingMessage = nil;
static id windowObserver        = nil;
static id sceneObserver         = nil;
static bool transitionRetryPending = false;

enum class FatalAlertState : uint8_t {
  Idle,
  Pending,
  Presenting,
  Presented,
  };
static FatalAlertState fatalAlertState = FatalAlertState::Idle;

static UIViewController* topViewController() {
  UIViewController* root = nil;
  for(UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    if(scene.activationState==UISceneActivationStateForegroundActive &&
       [scene isKindOfClass:[UIWindowScene class]]) {
      UIWindowScene* ws = (UIWindowScene*)scene;
      for(UIWindow* w in ws.windows)
        if(w.isKeyWindow) { root = w.rootViewController; break; }
      }
    if(root!=nil)
      break;
    }
  while(root.presentedViewController!=nil) {
    if(root.presentedViewController.isBeingDismissed)
      return root.presentedViewController;
    root = root.presentedViewController;
    }
  return root;
  }

static void tryPresentFatal();

static void retryAfterTransition(UIViewController* controller) {
  if(transitionRetryPending)
    return;
  transitionRetryPending = true;
  id<UIViewControllerTransitionCoordinator> coordinator = controller.transitionCoordinator;
  if(coordinator!=nil) {
    const BOOL registered =
        [coordinator animateAlongsideTransition:nil
                                    completion:^(id<UIViewControllerTransitionCoordinatorContext> context) {
                                      (void)context;
                                      transitionRetryPending = false;
                                      tryPresentFatal();
                                    }];
    if(registered)
      return;
    }
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW,int64_t(50*NSEC_PER_MSEC)),
                 dispatch_get_main_queue(), ^{
                   transitionRetryPending = false;
                   tryPresentFatal();
                 });
  }

static void observeWindowReadiness() {
  if(windowObserver!=nil || sceneObserver!=nil)
    return;
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  windowObserver = [center addObserverForName:UIWindowDidBecomeKeyNotification
                                       object:nil
                                        queue:[NSOperationQueue mainQueue]
                                   usingBlock:^(NSNotification*) {
                                     tryPresentFatal();
                                   }];
  sceneObserver = [center addObserverForName:UISceneDidActivateNotification
                                      object:nil
                                       queue:[NSOperationQueue mainQueue]
                                  usingBlock:^(NSNotification*) {
                                    tryPresentFatal();
                                  }];
  }

static void stopObservingWindowReadiness() {
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  if(windowObserver!=nil)
    [center removeObserver:windowObserver];
  if(sceneObserver!=nil)
    [center removeObserver:sceneObserver];
  windowObserver = nil;
  sceneObserver  = nil;
  }

static void clearPendingFatal() {
  stopObservingWindowReadiness();
  [pendingTitle release];
  [pendingMessage release];
  pendingTitle   = nil;
  pendingMessage = nil;
  }

static void finishPresentation(UIAlertController* alert,
                               UIViewController* presenter) {
  if(fatalAlertState!=FatalAlertState::Presenting)
    return;
  if(presenter.presentedViewController==alert &&
     alert.presentingViewController==presenter) {
    fatalAlertState = FatalAlertState::Presented;
    clearPendingFatal();
    return;
    }
  fatalAlertState = FatalAlertState::Pending;
  observeWindowReadiness();
  retryAfterTransition(presenter);
  }

static void tryPresentFatal() {
  if(fatalAlertState!=FatalAlertState::Pending ||
     pendingTitle==nil || pendingMessage==nil)
    return;
  UIViewController* root = topViewController();
  if(root==nil) {
    observeWindowReadiness();
    return;
    }
  if(root.isBeingPresented || root.isBeingDismissed ||
     root.transitionCoordinator!=nil || root.view.window==nil) {
    retryAfterTransition(root);
    return;
    }

  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:pendingTitle
                                          message:pendingMessage
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:nil]];
  fatalAlertState = FatalAlertState::Presenting;
  [root presentViewController:alert animated:YES completion:^{
    finishPresentation(alert,root);
    }];
  dispatch_async(dispatch_get_main_queue(), ^{
    finishPresentation(alert,root);
    });
  }

static NSString* retainedUtf8(const char* text, NSString* fallback) {
  NSString* value = [[NSString alloc] initWithUTF8String:(text ? text : "")];
  if(value==nil)
    value = [[NSString alloc] initWithString:fallback];
  return value;
  }

void SystemMsg::fatal(const char* title, const char* message) {
  NSString* t = retainedUtf8(title,  @"Fatal error");
  NSString* m = retainedUtf8(message,@"The error message is not valid UTF-8.");
  dispatch_async(dispatch_get_main_queue(), ^{
    // A fatal path is terminal. Keep the first message until it is presented
    // instead of replacing it or stacking alerts during startup transitions.
    if(fatalAlertState==FatalAlertState::Idle) {
      pendingTitle   = [t copy];
      pendingMessage = [m copy];
      fatalAlertState = FatalAlertState::Pending;
      tryPresentFatal();
      }
    [t release];
    [m release];
    });
  }

#endif
