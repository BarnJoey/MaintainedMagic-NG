Scriptname _m3MaintainMenuEffect extends ActiveMagicEffect  

GlobalVariable Property IsMaintenanceModeEnabled  Auto  
Sound Property PlaySound  Auto  
Message Property MaintainMessage  Auto  

Event OnEffectStart(Actor akTarget, Actor akCaster)
	PlaySound.Play(akTarget)
	IsMaintenanceModeEnabled.SetValueInt(1) 
	MaintainMessage.ShowAsHelpMessage("_m3MaintainMessage", 5, 10, 1)
EndEvent
Event OnEffectFinish(Actor akTarget, Actor akCaster)
	IsMaintenanceModeEnabled.SetValueInt(0) 
	Message.ResetHelpMessage("_m3MaintainMessage")
EndEvent



