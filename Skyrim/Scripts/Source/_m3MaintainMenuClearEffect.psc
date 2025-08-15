Scriptname _m3MaintainMenuClearEffect extends ActiveMagicEffect  

GlobalVariable Property IsDispelAllRequested  Auto  
Spell Property ThisSpell  Auto  

Event OnEffectStart(Actor akTarget, Actor akCaster)
		IsDispelAllRequested.SetValueInt(1) 
		akTarget.DispelSpell(ThisSpell)
EndEvent

