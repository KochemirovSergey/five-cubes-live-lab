#include "CoreMinimal.h"
#include "Async/Async.h"
#include "Mac/MacSystemIncludes.h"
#include "Apple/PreAppleSystemHeaders.h"
#import <AVFoundation/AVFoundation.h>
#include "Apple/PostAppleSystemHeaders.h"
void RequestLabMicrophone(TFunction<void(bool)> Callback)
{
    auto Shared=MakeShared<TFunction<void(bool)>>(MoveTemp(Callback));
    const auto Deliver=[Shared](bool Granted){ AsyncTask(ENamedThreads::GameThread,[Shared,Granted](){(*Shared)(Granted);}); };
    AVAuthorizationStatus Status=[AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
    if(Status==AVAuthorizationStatusAuthorized){Deliver(true);return;}
    if(Status!=AVAuthorizationStatusNotDetermined){Deliver(false);return;}
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL Granted){Deliver(Granted);}];
}
