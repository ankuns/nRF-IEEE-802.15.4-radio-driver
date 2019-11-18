// This file provides DebugLogDecoder "class"

var DebugLogDecoder = function(modules, globalEvents, functionIdToNameMap)
{
    this.modules = modules;
    this.globalEvents = globalEvents;
    this.functionIdToNameMap = functionIdToNameMap;
}

DebugLogDecoder.prototype.createUnknownModule = function(moduleId)
{
    var module = {};
    module.name = "Unknown (" + moduleId + ")";
    module.moduleId = moduleId;
    return module;
}

DebugLogDecoder.prototype.createUnknownLocalEvent = function(eventId)
{
    var result = {};
    result.eventId = eventId ;
    result.text = "Unknown local event (" + eventId + ")";
    return result;
}

DebugLogDecoder.prototype.createUnknownGlobalEvent = function(eventId)
{
    var result = {};
    result.eventId = eventId ;
    result.text = "Unknown global event (" + eventId + ")";
    return result;
}

DebugLogDecoder.prototype.createUnknownFunctionObj = function(functionId)
{
    var result = {};
    result.id = functionId ;
    result.name = "Unknown function (" + functionId + ")";
    return result;
}

DebugLogDecoder.prototype.searchArrayObjectsByPropertyId = function(arr, id)
{
    if (!Array.isArray(arr)) {
        return null;
    }
    var result = arr.find(obj => obj.id == id);
    if (result === undefined) {
        result = null;
    }
    return result;
}

DebugLogDecoder.prototype.enumValueToText = function(arr, value)
{
    if (!Array.isArray(arr)) {
        return "Unknown enum (" + value + ")";
    }
    
    var result = arr.find(obj => obj.value == value);
    if (result === undefined) {
        result = "Unknown enum (" + value + ")";
    }
    else {
        result = result.text;
    }
    return result;
}

DebugLogDecoder.prototype.getModuleById = function(moduleId)
{
    var module = this.searchArrayObjectsByPropertyId(this.modules, moduleId);
    if (module == null) 
    {
        module = this.createUnknownModule(moduleId);
    }
    return module;
}

DebugLogDecoder.prototype.getLocalEventById = function(module, eventId)
{
    var result = this.searchArrayObjectsByPropertyId(module.localEvents, eventId);
    if (result == null)
    {
        result = this.createUnknownLocalEvent(eventId);
    }
    return result;
}

DebugLogDecoder.prototype.getGlobalEventById = function(eventId)
{
    var result = this.searchArrayObjectsByPropertyId(this.globalEvents, eventId);
    if (result == null)
    {
        result = this.createUnknownGlobalEvent(eventId);
    }
    return result;
}

DebugLogDecoder.prototype.getEventText = function(event, eventParam)
{
    var result = "" ;
    result += event.text ;
    if (event.paramType != null) 
    {
        result += " ";
        switch (event.paramType) 
        {
            case "uint":
                result += eventParam.toString();
                break;
            case "enum":
                result += this.enumValueToText(event.enumValues, eventParam);
                break;
            default:
				// Possibly you have typo or introduced new paramType value not handled here
				throw "Internal error, unknown event.paramType";
        }
    }
    return result;
}

DebugLogDecoder.prototype.getFunctionObjById = function(functionId)
{
    var result = this.searchArrayObjectsByPropertyId(this.functionIdToNameMap, functionId);
    if (result == null) {
        result = this.createUnknownFunctionObj(functionId);
    }
    return result;
}

DebugLogDecoder.prototype.parseEventCode = function(eventCode)
{
    var result = {};
    
    result.eventType = (eventCode >> 28) & 0xF;
    switch(result.eventType)
    {
        case 1:
            result.module = this.getModuleById((eventCode >> 22) & 0x3F);            
            result.functionId = (eventCode >> 0) & 0x3FFFFF;
            result.functionObj = this.getFunctionObjById(result.functionId);
            result.text = "Enter: " + result.functionObj.name;
            break;
            
        case 2:
            result.module = this.getModuleById((eventCode >> 22) & 0x3F);            
            result.functionId = (eventCode >> 0) & 0x3FFFFF;
            result.functionObj = this.getFunctionObjById(result.functionId);
            result.text = "Exit: " + result.functionObj.name;
            break;
            
        case 3:
            result.module = this.getModuleById((eventCode >> 22) & 0x3F);
            result.localEvent = this.getLocalEventById(result.module, (eventCode >> 16) & 0x3F);
            result.localEventParam = (eventCode >> 0) & 0xFFFF;
            result.text = "Event: " + this.getEventText(result.localEvent, result.localEventParam);
            break;
            
        case 4:
            result.module = this.getModuleById((eventCode >> 22) & 0x3F);
            result.globalEvent = this.getGlobalEventById((eventCode >> 16) & 0x3F);
            result.globalEventParam = (eventCode >> 0) & 0xFFFF;
            result.text = "Event: " + this.getEventText(result.globalEvent, result.globalEventParam);
            break;
            
        default:
            result.module = this.createUnknownModule(0);
            result.text = "Unknown event type (" + eventCode + ")";
            break;
    }
    
    return result;
}