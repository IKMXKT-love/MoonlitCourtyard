#include "EmmaMemorySave.h"
bool UEmmaMemorySave::Valid() const
{
    return Version==1
        && VisitCount>=0 && LastVisitUtc.GetTicks()>=0 && LastVisitUtc<=FDateTime::UtcNow()+FTimespan::FromMinutes(5)
        && (LastMood==TEXT("calm") || LastMood==TEXT("happy") || LastMood==TEXT("sleepy") || LastMood==TEXT("lonely"));
}
