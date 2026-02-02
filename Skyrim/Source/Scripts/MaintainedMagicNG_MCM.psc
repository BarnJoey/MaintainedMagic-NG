Scriptname MaintainedMagicNG_MCM extends MCM_ConfigBase

; ==================================================
; Runtime-only MCM fields (NOT persisted)
; ==================================================

String Property sSpellName_1 Auto
Bool   Property bSpellFX_1 Auto
Bool   Property bSlotVisible_1 Auto

String Property sSpellName_2 Auto
Bool   Property bSpellFX_2 Auto
Bool   Property bSlotVisible_2 Auto

String Property sSpellName_3 Auto
Bool   Property bSpellFX_3 Auto
Bool   Property bSlotVisible_3 Auto

String Property sSpellName_4 Auto
Bool   Property bSpellFX_4 Auto
Bool   Property bSlotVisible_4 Auto

String Property sSpellName_5 Auto
Bool   Property bSpellFX_5 Auto
Bool   Property bSlotVisible_5 Auto

String Property sSpellName_6 Auto
Bool   Property bSpellFX_6 Auto
Bool   Property bSlotVisible_6 Auto

String Property sSpellName_7 Auto
Bool   Property bSpellFX_7 Auto
Bool   Property bSlotVisible_7 Auto

String Property sSpellName_8 Auto
Bool   Property bSpellFX_8 Auto
Bool   Property bSlotVisible_8 Auto

String Property sSpellName_9 Auto
Bool   Property bSpellFX_9 Auto
Bool   Property bSlotVisible_9 Auto

String Property sSpellName_10 Auto
Bool   Property bSpellFX_10 Auto
Bool   Property bSlotVisible_10 Auto

String Property sSpellName_11 Auto
Bool   Property bSpellFX_11 Auto
Bool   Property bSlotVisible_11 Auto

String Property sSpellName_12 Auto
Bool   Property bSpellFX_12 Auto
Bool   Property bSlotVisible_12 Auto

String Property sSpellName_13 Auto
Bool   Property bSpellFX_13 Auto
Bool   Property bSlotVisible_13 Auto

String Property sSpellName_14 Auto
Bool   Property bSpellFX_14 Auto
Bool   Property bSlotVisible_14 Auto

String Property sSpellName_15 Auto
Bool   Property bSpellFX_15 Auto
Bool   Property bSlotVisible_15 Auto

String Property sSpellName_16 Auto
Bool   Property bSpellFX_16 Auto
Bool   Property bSlotVisible_16 Auto

String Property sSpellName_17 Auto
Bool   Property bSpellFX_17 Auto
Bool   Property bSlotVisible_17 Auto

String Property sSpellName_18 Auto
Bool   Property bSpellFX_18 Auto
Bool   Property bSlotVisible_18 Auto

String Property sSpellName_19 Auto
Bool   Property bSpellFX_19 Auto
Bool   Property bSlotVisible_19 Auto

String Property sSpellName_20 Auto
Bool   Property bSpellFX_20 Auto
Bool   Property bSlotVisible_20 Auto

String Property sSpellName_21 Auto
Bool   Property bSpellFX_21 Auto
Bool   Property bSlotVisible_21 Auto

String Property sSpellName_22 Auto
Bool   Property bSpellFX_22 Auto
Bool   Property bSlotVisible_22 Auto

String Property sSpellName_23 Auto
Bool   Property bSpellFX_23 Auto
Bool   Property bSlotVisible_23 Auto

String Property sSpellName_24 Auto
Bool   Property bSpellFX_24 Auto
Bool   Property bSlotVisible_24 Auto

String Property sSpellName_25 Auto
Bool   Property bSpellFX_25 Auto
Bool   Property bSlotVisible_25 Auto

String Property sSpellName_26 Auto
Bool   Property bSpellFX_26 Auto
Bool   Property bSlotVisible_26 Auto

String Property sSpellName_27 Auto
Bool   Property bSpellFX_27 Auto
Bool   Property bSlotVisible_27 Auto

String Property sSpellName_28 Auto
Bool   Property bSpellFX_28 Auto
Bool   Property bSlotVisible_28 Auto

String Property sSpellName_29 Auto
Bool   Property bSpellFX_29 Auto
Bool   Property bSlotVisible_29 Auto

String Property sSpellName_30 Auto
Bool   Property bSpellFX_30 Auto
Bool   Property bSlotVisible_30 Auto

String Property sSpellName_31 Auto
Bool   Property bSpellFX_31 Auto
Bool   Property bSlotVisible_31 Auto

String Property sSpellName_32 Auto
Bool   Property bSpellFX_32 Auto
Bool   Property bSlotVisible_32 Auto


; =========================
; Page handling
; =========================

Event OnConfigOpen()
    Debug.Trace("[MaintainedMagicNG] MCM opened")

    RegisterForModEvent("MaintainedMagic_RuntimeSpell", "OnRuntimeSpell")

    ClearRuntimeSlots()
    SendModEvent("MaintainedMagic_RequestRuntimeSpells")
EndEvent


Event OnConfigClose()
    Debug.Trace("[MaintainedMagicNG] MCM closing, committing runtime FX states")

    CommitRuntimeSlot(sSpellName_1,  bSpellFX_1,  bSlotVisible_1)
    CommitRuntimeSlot(sSpellName_2,  bSpellFX_2,  bSlotVisible_2)
    CommitRuntimeSlot(sSpellName_3,  bSpellFX_3,  bSlotVisible_3)
    CommitRuntimeSlot(sSpellName_4,  bSpellFX_4,  bSlotVisible_4)
    CommitRuntimeSlot(sSpellName_5,  bSpellFX_5,  bSlotVisible_5)
    CommitRuntimeSlot(sSpellName_6,  bSpellFX_6,  bSlotVisible_6)
    CommitRuntimeSlot(sSpellName_7,  bSpellFX_7,  bSlotVisible_7)
    CommitRuntimeSlot(sSpellName_8,  bSpellFX_8,  bSlotVisible_8)
    CommitRuntimeSlot(sSpellName_9,  bSpellFX_9,  bSlotVisible_9)
    CommitRuntimeSlot(sSpellName_10, bSpellFX_10, bSlotVisible_10)
    CommitRuntimeSlot(sSpellName_11, bSpellFX_11, bSlotVisible_11)
    CommitRuntimeSlot(sSpellName_12, bSpellFX_12, bSlotVisible_12)
    CommitRuntimeSlot(sSpellName_13, bSpellFX_13, bSlotVisible_13)
    CommitRuntimeSlot(sSpellName_14, bSpellFX_14, bSlotVisible_14)
    CommitRuntimeSlot(sSpellName_15, bSpellFX_15, bSlotVisible_15)
    CommitRuntimeSlot(sSpellName_16, bSpellFX_16, bSlotVisible_16)
    CommitRuntimeSlot(sSpellName_17, bSpellFX_17, bSlotVisible_17)
    CommitRuntimeSlot(sSpellName_18, bSpellFX_18, bSlotVisible_18)
    CommitRuntimeSlot(sSpellName_19, bSpellFX_19, bSlotVisible_19)
    CommitRuntimeSlot(sSpellName_20, bSpellFX_20, bSlotVisible_20)
    CommitRuntimeSlot(sSpellName_21, bSpellFX_21, bSlotVisible_21)
    CommitRuntimeSlot(sSpellName_22, bSpellFX_22, bSlotVisible_22)
    CommitRuntimeSlot(sSpellName_23, bSpellFX_23, bSlotVisible_23)
    CommitRuntimeSlot(sSpellName_24, bSpellFX_24, bSlotVisible_24)
    CommitRuntimeSlot(sSpellName_25, bSpellFX_25, bSlotVisible_25)
    CommitRuntimeSlot(sSpellName_26, bSpellFX_26, bSlotVisible_26)
    CommitRuntimeSlot(sSpellName_27, bSpellFX_27, bSlotVisible_27)
    CommitRuntimeSlot(sSpellName_28, bSpellFX_28, bSlotVisible_28)
    CommitRuntimeSlot(sSpellName_29, bSpellFX_29, bSlotVisible_29)
    CommitRuntimeSlot(sSpellName_30, bSpellFX_30, bSlotVisible_30)
    CommitRuntimeSlot(sSpellName_31, bSpellFX_31, bSlotVisible_31)
    CommitRuntimeSlot(sSpellName_32, bSpellFX_32, bSlotVisible_32)
EndEvent


; =========================
; Incoming data from SKSE
; =========================

Event OnRuntimeSpell(string eventName, string payload, float fxState, Form sender)
    int sep = StringUtil.Find(payload, "|")
    if sep < 0
        Debug.Trace("[MaintainedMagicNG] Malformed runtime payload: " + payload)
        return
    endif

    string indexStr = StringUtil.Substring(payload, 0, sep)
    string spellName = StringUtil.Substring(payload, sep + 1)

    int index = indexStr as int
    bool fxEnabled = (fxState > 0.0)

    Debug.Trace("[MaintainedMagicNG] Slot " + index + ": " + spellName + " FX=" + fxEnabled)

    AssignRuntimeSlot(index, spellName, fxEnabled)

    ForcePageReset()
EndEvent


; =========================
; Helpers
; =========================

Function AssignRuntimeSlot(int index, string spellName, bool fxEnabled)
    if spellName == ""
        return
    endif

    if index == 1
        sSpellName_1 = spellName
        bSpellFX_1 = fxEnabled
        bSlotVisible_1 = true
    elseif index == 2
        sSpellName_2 = spellName
        bSpellFX_2 = fxEnabled
        bSlotVisible_2 = true
    elseif index == 3
        sSpellName_3 = spellName
        bSpellFX_3 = fxEnabled
        bSlotVisible_3 = true
    elseif index == 4
        sSpellName_4 = spellName
        bSpellFX_4 = fxEnabled
        bSlotVisible_4 = true
    elseif index == 5
        sSpellName_5 = spellName
        bSpellFX_5 = fxEnabled
        bSlotVisible_5 = true
    elseif index == 6
        sSpellName_6 = spellName
        bSpellFX_6 = fxEnabled
        bSlotVisible_6 = true
    elseif index == 7
        sSpellName_7 = spellName
        bSpellFX_7 = fxEnabled
        bSlotVisible_7 = true
    elseif index == 8
        sSpellName_8 = spellName
        bSpellFX_8 = fxEnabled
        bSlotVisible_8 = true
    elseif index == 9
        sSpellName_9 = spellName
        bSpellFX_9 = fxEnabled
        bSlotVisible_9 = true
    elseif index == 10
        sSpellName_10 = spellName
        bSpellFX_10 = fxEnabled
        bSlotVisible_10 = true
    elseif index == 11
        sSpellName_11 = spellName
        bSpellFX_11 = fxEnabled
        bSlotVisible_11 = true
    elseif index == 12
        sSpellName_12 = spellName
        bSpellFX_12 = fxEnabled
        bSlotVisible_12 = true
    elseif index == 13
        sSpellName_13 = spellName
        bSpellFX_13 = fxEnabled
        bSlotVisible_13 = true
    elseif index == 14
        sSpellName_14 = spellName
        bSpellFX_14 = fxEnabled
        bSlotVisible_14 = true
    elseif index == 15
        sSpellName_15 = spellName
        bSpellFX_15 = fxEnabled
        bSlotVisible_15 = true
    elseif index == 16
        sSpellName_16 = spellName
        bSpellFX_16 = fxEnabled
        bSlotVisible_16 = true
    elseif index == 17
        sSpellName_17 = spellName
        bSpellFX_17 = fxEnabled
        bSlotVisible_17 = true
    elseif index == 18
        sSpellName_18 = spellName
        bSpellFX_18 = fxEnabled
        bSlotVisible_18 = true
    elseif index == 19
        sSpellName_19 = spellName
        bSpellFX_19 = fxEnabled
        bSlotVisible_19 = true
    elseif index == 20
        sSpellName_20 = spellName
        bSpellFX_20 = fxEnabled
        bSlotVisible_20 = true
    elseif index == 21
        sSpellName_21 = spellName
        bSpellFX_21 = fxEnabled
        bSlotVisible_21 = true
    elseif index == 22
        sSpellName_22 = spellName
        bSpellFX_22 = fxEnabled
        bSlotVisible_22 = true
    elseif index == 23
        sSpellName_23 = spellName
        bSpellFX_23 = fxEnabled
        bSlotVisible_23 = true
    elseif index == 24
        sSpellName_24 = spellName
        bSpellFX_24 = fxEnabled
        bSlotVisible_24 = true
    elseif index == 25
        sSpellName_25 = spellName
        bSpellFX_25 = fxEnabled
        bSlotVisible_25 = true
    elseif index == 26
        sSpellName_26 = spellName
        bSpellFX_26 = fxEnabled
        bSlotVisible_26 = true
    elseif index == 27
        sSpellName_27 = spellName
        bSpellFX_27 = fxEnabled
        bSlotVisible_27 = true
    elseif index == 28
        sSpellName_28 = spellName
        bSpellFX_28 = fxEnabled
        bSlotVisible_28 = true
    elseif index == 29
        sSpellName_29 = spellName
        bSpellFX_29 = fxEnabled
        bSlotVisible_29 = true
    elseif index == 30
        sSpellName_30 = spellName
        bSpellFX_30 = fxEnabled
        bSlotVisible_30 = true
    elseif index == 31
        sSpellName_31 = spellName
        bSpellFX_31 = fxEnabled
        bSlotVisible_31 = true
    elseif index == 32
        sSpellName_32 = spellName
        bSpellFX_32 = fxEnabled
        bSlotVisible_32 = true
    endif
EndFunction


Function CommitRuntimeSlot(string spellName, bool fxEnabled, bool slotVisible)
    if !slotVisible
        return
    endif

    if spellName == ""
        return
    endif

    float fxValue = 0.0
    if fxEnabled
        fxValue = 1.0
    endif

    SendModEvent("MaintainedMagic_RuntimeFXCommit", spellName, fxValue)
    Debug.Trace("[MaintainedMagicNG] Committed FX state: " + spellName + " = " + fxEnabled)
EndFunction


Function ClearRuntimeSlots()
    sSpellName_1 = ""
    bSpellFX_1 = false
    bSlotVisible_1 = false

    sSpellName_2 = ""
    bSpellFX_2 = false
    bSlotVisible_2 = false

    sSpellName_3 = ""
    bSpellFX_3 = false
    bSlotVisible_3 = false

    sSpellName_4 = ""
    bSpellFX_4 = false
    bSlotVisible_4 = false

    sSpellName_5 = ""
    bSpellFX_5 = false
    bSlotVisible_5 = false

    sSpellName_6 = ""
    bSpellFX_6 = false
    bSlotVisible_6 = false

    sSpellName_7 = ""
    bSpellFX_7 = false
    bSlotVisible_7 = false

    sSpellName_8 = ""
    bSpellFX_8 = false
    bSlotVisible_8 = false

    sSpellName_9 = ""
    bSpellFX_9 = false
    bSlotVisible_9 = false

    sSpellName_10 = ""
    bSpellFX_10 = false
    bSlotVisible_10 = false

    sSpellName_11 = ""
    bSpellFX_11 = false
    bSlotVisible_11 = false

    sSpellName_12 = ""
    bSpellFX_12 = false
    bSlotVisible_12 = false

    sSpellName_13 = ""
    bSpellFX_13 = false
    bSlotVisible_13 = false

    sSpellName_14 = ""
    bSpellFX_14 = false
    bSlotVisible_14 = false

    sSpellName_15 = ""
    bSpellFX_15 = false
    bSlotVisible_15 = false

    sSpellName_16 = ""
    bSpellFX_16 = false
    bSlotVisible_16 = false

    sSpellName_17 = ""
    bSpellFX_17 = false
    bSlotVisible_17 = false

    sSpellName_18 = ""
    bSpellFX_18 = false
    bSlotVisible_18 = false

    sSpellName_19 = ""
    bSpellFX_19 = false
    bSlotVisible_19 = false

    sSpellName_20 = ""
    bSpellFX_20 = false
    bSlotVisible_20 = false

    sSpellName_21 = ""
    bSpellFX_21 = false
    bSlotVisible_21 = false

    sSpellName_22 = ""
    bSpellFX_22 = false
    bSlotVisible_22 = false

    sSpellName_23 = ""
    bSpellFX_23 = false
    bSlotVisible_23 = false

    sSpellName_24 = ""
    bSpellFX_24 = false
    bSlotVisible_24 = false

    sSpellName_25 = ""
    bSpellFX_25 = false
    bSlotVisible_25 = false

    sSpellName_26 = ""
    bSpellFX_26 = false
    bSlotVisible_26 = false

    sSpellName_27 = ""
    bSpellFX_27 = false
    bSlotVisible_27 = false

    sSpellName_28 = ""
    bSpellFX_28 = false
    bSlotVisible_28 = false

    sSpellName_29 = ""
    bSpellFX_29 = false
    bSlotVisible_29 = false

    sSpellName_30 = ""
    bSpellFX_30 = false
    bSlotVisible_30 = false

    sSpellName_31 = ""
    bSpellFX_31 = false
    bSlotVisible_31 = false

    sSpellName_32 = ""
    bSpellFX_32 = false
    bSlotVisible_32 = false
EndFunction
