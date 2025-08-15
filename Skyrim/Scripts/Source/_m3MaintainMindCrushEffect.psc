Scriptname _m3MaintainMindCrushEffect extends ActiveMagicEffect  

Keyword Property InfiniteKeyword Auto
Message Property MindCrushMessage Auto
Sound Property PlaySound  Auto  

Event OnEffectStart(Actor akTarget, Actor akCaster)
	If akTarget.GetRace() == WerewolfRace ||  akTarget.GetRace() == VampireLordRace
		return
	EndIf

	PlaySound.Play(akTarget)
	Debug.SendAnimationEvent(akTarget as ObjectReference, "StaggerStart")

	int spellCount = akTarget.GetSpellCount() - 1
	while spellCount >= 0
		Spell checkedSpell = akTarget.GetNthSpell(spellCount)
		if checkedSpell && checkedSpell.HasKeyword(InfiniteKeyword)
			akTarget.RemoveSpell(checkedSpell)
		endif
		spellCount -= 1
	endwhile

	MindCrushMessage.ShowAsHelpMessage("_m3MaintainMindCrushMessage", 5, 10, 1)
EndEvent

Event OnEffectFinish(Actor akTarget, Actor akCaster)
	Message.ResetHelpMessage("_m3MaintainMindCrushMessage")
EndEvent


Race Property VampireLordRace  Auto  

Race Property WerewolfRace  Auto  
