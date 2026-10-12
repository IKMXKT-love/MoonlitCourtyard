#include "CourtyardController.h"
#include "CourtyardContentLife.h"
void ACourtyardController::CourtyardReviewContent(int32 Event)
{
 if(ContentLayer.IsValid())ContentLayer->Review(Event);
}
