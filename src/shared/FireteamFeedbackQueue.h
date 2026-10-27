#ifndef __FIRETEAM_FEEDBACK_QUEUE_H__
#define __FIRETEAM_FEEDBACK_QUEUE_H__
#include "FireteamCombatRules.h"
namespace FTFeedbackRules
{
    class RecentQueue
    {
    public:
        RecentQueue() : count(0) {}
        void Clear() { count=0; }
        void Push(unsigned char kind, float now)
        {
            if(!kind || !FTCombatRules::Finite(now)) return;
            for(unsigned i=0;i<count;++i) if(kinds[i]==kind)
            { times[i]=now; return; }
            if(count==4) EraseFirst();
            kinds[count]=kind; times[count]=now; ++count;
        }
        unsigned char Pop(float now)
        {
            if(!FTCombatRules::Finite(now)) { Clear(); return 0; }
            while(count)
            {
                const unsigned char kind=kinds[0];
                const float age=now-times[0];
                EraseFirst();
                if(age>=0 && age<=2.5f) return kind;
            }
            return 0;
        }
    private:
        void EraseFirst()
        {
            for(unsigned i=1;i<count;++i) { kinds[i-1]=kinds[i]; times[i-1]=times[i]; }
            if(count) --count;
        }
        unsigned char kinds[4]; float times[4]; unsigned count;
    };
}
#endif
