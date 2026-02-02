Scriptname _m3MaintainAddPerkQuest extends Quest  

Perk Property ThePerk Auto
SPELL Property TheToggleSpell  Auto  

Event OnInit()
	Game.GetPlayer().AddPerk(ThePerk)
	Game.GetPlayer().AddSPell(TheToggleSpell)
EndEvent
