/*eslint-disable block-scoped-var, id-length, no-control-regex, no-magic-numbers, no-prototype-builtins, no-redeclare, no-shadow, no-var, sort-vars*/
(function($protobuf) {
    "use strict";

    // Common aliases
    var $Reader = $protobuf.Reader, $Writer = $protobuf.Writer, $util = $protobuf.util;
    
    // Exported root namespace
    var $root = $protobuf.roots["prodigy-api"] || ($protobuf.roots["prodigy-api"] = {});
    
    $root.prodigy = (function() {
    
        /**
         * Namespace prodigy.
         * @exports prodigy
         * @namespace
         */
        var prodigy = {};
    
        prodigy.api = (function() {
    
            /**
             * Namespace api.
             * @memberof prodigy
             * @namespace
             */
            var api = {};
    
            api.v1 = (function() {
    
                /**
                 * Namespace v1.
                 * @memberof prodigy.api
                 * @namespace
                 */
                var v1 = {};
    
                v1.ActionInfo = (function() {
    
                    /**
                     * Properties of an ActionInfo.
                     * @memberof prodigy.api.v1
                     * @interface IActionInfo
                     * @property {string|null} [id] ActionInfo id
                     * @property {string|null} [label] ActionInfo label
                     * @property {boolean|null} [clientOwned] ActionInfo clientOwned
                     */
    
                    /**
                     * Constructs a new ActionInfo.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents an ActionInfo.
                     * @implements IActionInfo
                     * @constructor
                     * @param {prodigy.api.v1.IActionInfo=} [properties] Properties to set
                     */
                    function ActionInfo(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * ActionInfo id.
                     * @member {string} id
                     * @memberof prodigy.api.v1.ActionInfo
                     * @instance
                     */
                    ActionInfo.prototype.id = "";
    
                    /**
                     * ActionInfo label.
                     * @member {string} label
                     * @memberof prodigy.api.v1.ActionInfo
                     * @instance
                     */
                    ActionInfo.prototype.label = "";
    
                    /**
                     * ActionInfo clientOwned.
                     * @member {boolean} clientOwned
                     * @memberof prodigy.api.v1.ActionInfo
                     * @instance
                     */
                    ActionInfo.prototype.clientOwned = false;
    
                    /**
                     * Creates a new ActionInfo instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.ActionInfo
                     * @static
                     * @param {prodigy.api.v1.IActionInfo=} [properties] Properties to set
                     * @returns {prodigy.api.v1.ActionInfo} ActionInfo instance
                     */
                    ActionInfo.create = function create(properties) {
                        return new ActionInfo(properties);
                    };
    
                    /**
                     * Encodes the specified ActionInfo message. Does not implicitly {@link prodigy.api.v1.ActionInfo.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.ActionInfo
                     * @static
                     * @param {prodigy.api.v1.IActionInfo} message ActionInfo message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    ActionInfo.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.id != null && Object.hasOwnProperty.call(message, "id"))
                            writer.uint32(/* id 1, wireType 2 =*/10).string(message.id);
                        if (message.label != null && Object.hasOwnProperty.call(message, "label"))
                            writer.uint32(/* id 2, wireType 2 =*/18).string(message.label);
                        if (message.clientOwned != null && Object.hasOwnProperty.call(message, "clientOwned"))
                            writer.uint32(/* id 3, wireType 0 =*/24).bool(message.clientOwned);
                        return writer;
                    };
    
                    /**
                     * Decodes an ActionInfo message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.ActionInfo
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.ActionInfo} ActionInfo
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    ActionInfo.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.ActionInfo();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.id = reader.string();
                                    break;
                                }
                            case 2: {
                                    message.label = reader.string();
                                    break;
                                }
                            case 3: {
                                    message.clientOwned = reader.bool();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies an ActionInfo message.
                     * @function verify
                     * @memberof prodigy.api.v1.ActionInfo
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    ActionInfo.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.id != null && Object.hasOwnProperty.call(message, "id"))
                            if (!$util.isString(message.id))
                                return "id: string expected";
                        if (message.label != null && Object.hasOwnProperty.call(message, "label"))
                            if (!$util.isString(message.label))
                                return "label: string expected";
                        if (message.clientOwned != null && Object.hasOwnProperty.call(message, "clientOwned"))
                            if (typeof message.clientOwned !== "boolean")
                                return "clientOwned: boolean expected";
                        return null;
                    };
    
                    /**
                     * Creates an ActionInfo message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.ActionInfo
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.ActionInfo} ActionInfo
                     */
                    ActionInfo.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.ActionInfo)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.ActionInfo: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.ActionInfo();
                        if (object.id != null)
                            message.id = String(object.id);
                        if (object.label != null)
                            message.label = String(object.label);
                        if (object.clientOwned != null)
                            message.clientOwned = Boolean(object.clientOwned);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from an ActionInfo message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.ActionInfo
                     * @static
                     * @param {prodigy.api.v1.ActionInfo} message ActionInfo
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    ActionInfo.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.id = "";
                            object.label = "";
                            object.clientOwned = false;
                        }
                        if (message.id != null && Object.hasOwnProperty.call(message, "id"))
                            object.id = message.id;
                        if (message.label != null && Object.hasOwnProperty.call(message, "label"))
                            object.label = message.label;
                        if (message.clientOwned != null && Object.hasOwnProperty.call(message, "clientOwned"))
                            object.clientOwned = message.clientOwned;
                        return object;
                    };
    
                    /**
                     * Converts this ActionInfo to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.ActionInfo
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    ActionInfo.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for ActionInfo
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.ActionInfo
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    ActionInfo.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.ActionInfo";
                    };
    
                    return ActionInfo;
                })();
    
                v1.ListActionsRequest = (function() {
    
                    /**
                     * Properties of a ListActionsRequest.
                     * @memberof prodigy.api.v1
                     * @interface IListActionsRequest
                     */
    
                    /**
                     * Constructs a new ListActionsRequest.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a ListActionsRequest.
                     * @implements IListActionsRequest
                     * @constructor
                     * @param {prodigy.api.v1.IListActionsRequest=} [properties] Properties to set
                     */
                    function ListActionsRequest(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * Creates a new ListActionsRequest instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.ListActionsRequest
                     * @static
                     * @param {prodigy.api.v1.IListActionsRequest=} [properties] Properties to set
                     * @returns {prodigy.api.v1.ListActionsRequest} ListActionsRequest instance
                     */
                    ListActionsRequest.create = function create(properties) {
                        return new ListActionsRequest(properties);
                    };
    
                    /**
                     * Encodes the specified ListActionsRequest message. Does not implicitly {@link prodigy.api.v1.ListActionsRequest.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.ListActionsRequest
                     * @static
                     * @param {prodigy.api.v1.IListActionsRequest} message ListActionsRequest message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    ListActionsRequest.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        return writer;
                    };
    
                    /**
                     * Decodes a ListActionsRequest message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.ListActionsRequest
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.ListActionsRequest} ListActionsRequest
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    ListActionsRequest.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.ListActionsRequest();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a ListActionsRequest message.
                     * @function verify
                     * @memberof prodigy.api.v1.ListActionsRequest
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    ListActionsRequest.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        return null;
                    };
    
                    /**
                     * Creates a ListActionsRequest message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.ListActionsRequest
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.ListActionsRequest} ListActionsRequest
                     */
                    ListActionsRequest.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.ListActionsRequest)
                            return object;
                        return new $root.prodigy.api.v1.ListActionsRequest();
                    };
    
                    /**
                     * Creates a plain object from a ListActionsRequest message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.ListActionsRequest
                     * @static
                     * @param {prodigy.api.v1.ListActionsRequest} message ListActionsRequest
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    ListActionsRequest.toObject = function toObject() {
                        return {};
                    };
    
                    /**
                     * Converts this ListActionsRequest to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.ListActionsRequest
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    ListActionsRequest.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for ListActionsRequest
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.ListActionsRequest
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    ListActionsRequest.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.ListActionsRequest";
                    };
    
                    return ListActionsRequest;
                })();
    
                v1.ListActionsResponse = (function() {
    
                    /**
                     * Properties of a ListActionsResponse.
                     * @memberof prodigy.api.v1
                     * @interface IListActionsResponse
                     * @property {Array.<prodigy.api.v1.IActionInfo>|null} [actions] ListActionsResponse actions
                     */
    
                    /**
                     * Constructs a new ListActionsResponse.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a ListActionsResponse.
                     * @implements IListActionsResponse
                     * @constructor
                     * @param {prodigy.api.v1.IListActionsResponse=} [properties] Properties to set
                     */
                    function ListActionsResponse(properties) {
                        this.actions = [];
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * ListActionsResponse actions.
                     * @member {Array.<prodigy.api.v1.IActionInfo>} actions
                     * @memberof prodigy.api.v1.ListActionsResponse
                     * @instance
                     */
                    ListActionsResponse.prototype.actions = $util.emptyArray;
    
                    /**
                     * Creates a new ListActionsResponse instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.ListActionsResponse
                     * @static
                     * @param {prodigy.api.v1.IListActionsResponse=} [properties] Properties to set
                     * @returns {prodigy.api.v1.ListActionsResponse} ListActionsResponse instance
                     */
                    ListActionsResponse.create = function create(properties) {
                        return new ListActionsResponse(properties);
                    };
    
                    /**
                     * Encodes the specified ListActionsResponse message. Does not implicitly {@link prodigy.api.v1.ListActionsResponse.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.ListActionsResponse
                     * @static
                     * @param {prodigy.api.v1.IListActionsResponse} message ListActionsResponse message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    ListActionsResponse.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.actions != null && message.actions.length)
                            for (var i = 0; i < message.actions.length; ++i)
                                $root.prodigy.api.v1.ActionInfo.encode(message.actions[i], writer.uint32(/* id 1, wireType 2 =*/10).fork(), q + 1).ldelim();
                        return writer;
                    };
    
                    /**
                     * Decodes a ListActionsResponse message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.ListActionsResponse
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.ListActionsResponse} ListActionsResponse
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    ListActionsResponse.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.ListActionsResponse();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    if (!(message.actions && message.actions.length))
                                        message.actions = [];
                                    message.actions.push($root.prodigy.api.v1.ActionInfo.decode(reader, reader.uint32(), undefined, long + 1));
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a ListActionsResponse message.
                     * @function verify
                     * @memberof prodigy.api.v1.ListActionsResponse
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    ListActionsResponse.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.actions != null && Object.hasOwnProperty.call(message, "actions")) {
                            if (!Array.isArray(message.actions))
                                return "actions: array expected";
                            for (var i = 0; i < message.actions.length; ++i) {
                                var error = $root.prodigy.api.v1.ActionInfo.verify(message.actions[i], long + 1);
                                if (error)
                                    return "actions." + error;
                            }
                        }
                        return null;
                    };
    
                    /**
                     * Creates a ListActionsResponse message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.ListActionsResponse
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.ListActionsResponse} ListActionsResponse
                     */
                    ListActionsResponse.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.ListActionsResponse)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.ListActionsResponse: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.ListActionsResponse();
                        if (object.actions) {
                            if (!Array.isArray(object.actions))
                                throw TypeError(".prodigy.api.v1.ListActionsResponse.actions: array expected");
                            message.actions = [];
                            for (var i = 0; i < object.actions.length; ++i) {
                                if (!$util.isObject(object.actions[i]))
                                    throw TypeError(".prodigy.api.v1.ListActionsResponse.actions: object expected");
                                message.actions[i] = $root.prodigy.api.v1.ActionInfo.fromObject(object.actions[i], long + 1);
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a ListActionsResponse message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.ListActionsResponse
                     * @static
                     * @param {prodigy.api.v1.ListActionsResponse} message ListActionsResponse
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    ListActionsResponse.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.arrays || options.defaults)
                            object.actions = [];
                        if (message.actions && message.actions.length) {
                            object.actions = [];
                            for (var j = 0; j < message.actions.length; ++j)
                                object.actions[j] = $root.prodigy.api.v1.ActionInfo.toObject(message.actions[j], options, q + 1);
                        }
                        return object;
                    };
    
                    /**
                     * Converts this ListActionsResponse to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.ListActionsResponse
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    ListActionsResponse.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for ListActionsResponse
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.ListActionsResponse
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    ListActionsResponse.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.ListActionsResponse";
                    };
    
                    return ListActionsResponse;
                })();
    
                v1.DispatchActionRequest = (function() {
    
                    /**
                     * Properties of a DispatchActionRequest.
                     * @memberof prodigy.api.v1
                     * @interface IDispatchActionRequest
                     * @property {string|null} [id] DispatchActionRequest id
                     * @property {string|null} [payloadJson] DispatchActionRequest payloadJson
                     */
    
                    /**
                     * Constructs a new DispatchActionRequest.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a DispatchActionRequest.
                     * @implements IDispatchActionRequest
                     * @constructor
                     * @param {prodigy.api.v1.IDispatchActionRequest=} [properties] Properties to set
                     */
                    function DispatchActionRequest(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * DispatchActionRequest id.
                     * @member {string} id
                     * @memberof prodigy.api.v1.DispatchActionRequest
                     * @instance
                     */
                    DispatchActionRequest.prototype.id = "";
    
                    /**
                     * DispatchActionRequest payloadJson.
                     * @member {string|null|undefined} payloadJson
                     * @memberof prodigy.api.v1.DispatchActionRequest
                     * @instance
                     */
                    DispatchActionRequest.prototype.payloadJson = null;
    
                    // OneOf field names bound to virtual getters and setters
                    var $oneOfFields;
    
                    // Virtual OneOf for proto3 optional field
                    Object.defineProperty(DispatchActionRequest.prototype, "_payloadJson", {
                        get: $util.oneOfGetter($oneOfFields = ["payloadJson"]),
                        set: $util.oneOfSetter($oneOfFields)
                    });
    
                    /**
                     * Creates a new DispatchActionRequest instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.DispatchActionRequest
                     * @static
                     * @param {prodigy.api.v1.IDispatchActionRequest=} [properties] Properties to set
                     * @returns {prodigy.api.v1.DispatchActionRequest} DispatchActionRequest instance
                     */
                    DispatchActionRequest.create = function create(properties) {
                        return new DispatchActionRequest(properties);
                    };
    
                    /**
                     * Encodes the specified DispatchActionRequest message. Does not implicitly {@link prodigy.api.v1.DispatchActionRequest.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.DispatchActionRequest
                     * @static
                     * @param {prodigy.api.v1.IDispatchActionRequest} message DispatchActionRequest message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    DispatchActionRequest.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.id != null && Object.hasOwnProperty.call(message, "id"))
                            writer.uint32(/* id 1, wireType 2 =*/10).string(message.id);
                        if (message.payloadJson != null && Object.hasOwnProperty.call(message, "payloadJson"))
                            writer.uint32(/* id 2, wireType 2 =*/18).string(message.payloadJson);
                        return writer;
                    };
    
                    /**
                     * Decodes a DispatchActionRequest message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.DispatchActionRequest
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.DispatchActionRequest} DispatchActionRequest
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    DispatchActionRequest.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.DispatchActionRequest();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.id = reader.string();
                                    break;
                                }
                            case 2: {
                                    message.payloadJson = reader.string();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a DispatchActionRequest message.
                     * @function verify
                     * @memberof prodigy.api.v1.DispatchActionRequest
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    DispatchActionRequest.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        var properties = {};
                        if (message.id != null && Object.hasOwnProperty.call(message, "id"))
                            if (!$util.isString(message.id))
                                return "id: string expected";
                        if (message.payloadJson != null && Object.hasOwnProperty.call(message, "payloadJson")) {
                            properties._payloadJson = 1;
                            if (!$util.isString(message.payloadJson))
                                return "payloadJson: string expected";
                        }
                        return null;
                    };
    
                    /**
                     * Creates a DispatchActionRequest message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.DispatchActionRequest
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.DispatchActionRequest} DispatchActionRequest
                     */
                    DispatchActionRequest.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.DispatchActionRequest)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.DispatchActionRequest: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.DispatchActionRequest();
                        if (object.id != null)
                            message.id = String(object.id);
                        if (object.payloadJson != null)
                            message.payloadJson = String(object.payloadJson);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a DispatchActionRequest message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.DispatchActionRequest
                     * @static
                     * @param {prodigy.api.v1.DispatchActionRequest} message DispatchActionRequest
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    DispatchActionRequest.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults)
                            object.id = "";
                        if (message.id != null && Object.hasOwnProperty.call(message, "id"))
                            object.id = message.id;
                        if (message.payloadJson != null && Object.hasOwnProperty.call(message, "payloadJson")) {
                            object.payloadJson = message.payloadJson;
                            if (options.oneofs)
                                object._payloadJson = "payloadJson";
                        }
                        return object;
                    };
    
                    /**
                     * Converts this DispatchActionRequest to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.DispatchActionRequest
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    DispatchActionRequest.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for DispatchActionRequest
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.DispatchActionRequest
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    DispatchActionRequest.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.DispatchActionRequest";
                    };
    
                    return DispatchActionRequest;
                })();
    
                v1.DispatchActionResponse = (function() {
    
                    /**
                     * Properties of a DispatchActionResponse.
                     * @memberof prodigy.api.v1
                     * @interface IDispatchActionResponse
                     * @property {boolean|null} [dispatched] DispatchActionResponse dispatched
                     */
    
                    /**
                     * Constructs a new DispatchActionResponse.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a DispatchActionResponse.
                     * @implements IDispatchActionResponse
                     * @constructor
                     * @param {prodigy.api.v1.IDispatchActionResponse=} [properties] Properties to set
                     */
                    function DispatchActionResponse(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * DispatchActionResponse dispatched.
                     * @member {boolean} dispatched
                     * @memberof prodigy.api.v1.DispatchActionResponse
                     * @instance
                     */
                    DispatchActionResponse.prototype.dispatched = false;
    
                    /**
                     * Creates a new DispatchActionResponse instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.DispatchActionResponse
                     * @static
                     * @param {prodigy.api.v1.IDispatchActionResponse=} [properties] Properties to set
                     * @returns {prodigy.api.v1.DispatchActionResponse} DispatchActionResponse instance
                     */
                    DispatchActionResponse.create = function create(properties) {
                        return new DispatchActionResponse(properties);
                    };
    
                    /**
                     * Encodes the specified DispatchActionResponse message. Does not implicitly {@link prodigy.api.v1.DispatchActionResponse.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.DispatchActionResponse
                     * @static
                     * @param {prodigy.api.v1.IDispatchActionResponse} message DispatchActionResponse message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    DispatchActionResponse.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.dispatched != null && Object.hasOwnProperty.call(message, "dispatched"))
                            writer.uint32(/* id 1, wireType 0 =*/8).bool(message.dispatched);
                        return writer;
                    };
    
                    /**
                     * Decodes a DispatchActionResponse message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.DispatchActionResponse
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.DispatchActionResponse} DispatchActionResponse
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    DispatchActionResponse.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.DispatchActionResponse();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.dispatched = reader.bool();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a DispatchActionResponse message.
                     * @function verify
                     * @memberof prodigy.api.v1.DispatchActionResponse
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    DispatchActionResponse.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.dispatched != null && Object.hasOwnProperty.call(message, "dispatched"))
                            if (typeof message.dispatched !== "boolean")
                                return "dispatched: boolean expected";
                        return null;
                    };
    
                    /**
                     * Creates a DispatchActionResponse message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.DispatchActionResponse
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.DispatchActionResponse} DispatchActionResponse
                     */
                    DispatchActionResponse.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.DispatchActionResponse)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.DispatchActionResponse: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.DispatchActionResponse();
                        if (object.dispatched != null)
                            message.dispatched = Boolean(object.dispatched);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a DispatchActionResponse message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.DispatchActionResponse
                     * @static
                     * @param {prodigy.api.v1.DispatchActionResponse} message DispatchActionResponse
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    DispatchActionResponse.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults)
                            object.dispatched = false;
                        if (message.dispatched != null && Object.hasOwnProperty.call(message, "dispatched"))
                            object.dispatched = message.dispatched;
                        return object;
                    };
    
                    /**
                     * Converts this DispatchActionResponse to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.DispatchActionResponse
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    DispatchActionResponse.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for DispatchActionResponse
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.DispatchActionResponse
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    DispatchActionResponse.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.DispatchActionResponse";
                    };
    
                    return DispatchActionResponse;
                })();
    
                v1.ActionSpec = (function() {
    
                    /**
                     * Properties of an ActionSpec.
                     * @memberof prodigy.api.v1
                     * @interface IActionSpec
                     * @property {string|null} [id] ActionSpec id
                     * @property {string|null} [label] ActionSpec label
                     */
    
                    /**
                     * Constructs a new ActionSpec.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents an ActionSpec.
                     * @implements IActionSpec
                     * @constructor
                     * @param {prodigy.api.v1.IActionSpec=} [properties] Properties to set
                     */
                    function ActionSpec(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * ActionSpec id.
                     * @member {string} id
                     * @memberof prodigy.api.v1.ActionSpec
                     * @instance
                     */
                    ActionSpec.prototype.id = "";
    
                    /**
                     * ActionSpec label.
                     * @member {string} label
                     * @memberof prodigy.api.v1.ActionSpec
                     * @instance
                     */
                    ActionSpec.prototype.label = "";
    
                    /**
                     * Creates a new ActionSpec instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.ActionSpec
                     * @static
                     * @param {prodigy.api.v1.IActionSpec=} [properties] Properties to set
                     * @returns {prodigy.api.v1.ActionSpec} ActionSpec instance
                     */
                    ActionSpec.create = function create(properties) {
                        return new ActionSpec(properties);
                    };
    
                    /**
                     * Encodes the specified ActionSpec message. Does not implicitly {@link prodigy.api.v1.ActionSpec.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.ActionSpec
                     * @static
                     * @param {prodigy.api.v1.IActionSpec} message ActionSpec message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    ActionSpec.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.id != null && Object.hasOwnProperty.call(message, "id"))
                            writer.uint32(/* id 1, wireType 2 =*/10).string(message.id);
                        if (message.label != null && Object.hasOwnProperty.call(message, "label"))
                            writer.uint32(/* id 2, wireType 2 =*/18).string(message.label);
                        return writer;
                    };
    
                    /**
                     * Decodes an ActionSpec message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.ActionSpec
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.ActionSpec} ActionSpec
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    ActionSpec.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.ActionSpec();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.id = reader.string();
                                    break;
                                }
                            case 2: {
                                    message.label = reader.string();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies an ActionSpec message.
                     * @function verify
                     * @memberof prodigy.api.v1.ActionSpec
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    ActionSpec.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.id != null && Object.hasOwnProperty.call(message, "id"))
                            if (!$util.isString(message.id))
                                return "id: string expected";
                        if (message.label != null && Object.hasOwnProperty.call(message, "label"))
                            if (!$util.isString(message.label))
                                return "label: string expected";
                        return null;
                    };
    
                    /**
                     * Creates an ActionSpec message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.ActionSpec
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.ActionSpec} ActionSpec
                     */
                    ActionSpec.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.ActionSpec)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.ActionSpec: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.ActionSpec();
                        if (object.id != null)
                            message.id = String(object.id);
                        if (object.label != null)
                            message.label = String(object.label);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from an ActionSpec message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.ActionSpec
                     * @static
                     * @param {prodigy.api.v1.ActionSpec} message ActionSpec
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    ActionSpec.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.id = "";
                            object.label = "";
                        }
                        if (message.id != null && Object.hasOwnProperty.call(message, "id"))
                            object.id = message.id;
                        if (message.label != null && Object.hasOwnProperty.call(message, "label"))
                            object.label = message.label;
                        return object;
                    };
    
                    /**
                     * Converts this ActionSpec to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.ActionSpec
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    ActionSpec.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for ActionSpec
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.ActionSpec
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    ActionSpec.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.ActionSpec";
                    };
    
                    return ActionSpec;
                })();
    
                v1.RegisterActionsRequest = (function() {
    
                    /**
                     * Properties of a RegisterActionsRequest.
                     * @memberof prodigy.api.v1
                     * @interface IRegisterActionsRequest
                     * @property {Array.<prodigy.api.v1.IActionSpec>|null} [actions] RegisterActionsRequest actions
                     */
    
                    /**
                     * Constructs a new RegisterActionsRequest.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a RegisterActionsRequest.
                     * @implements IRegisterActionsRequest
                     * @constructor
                     * @param {prodigy.api.v1.IRegisterActionsRequest=} [properties] Properties to set
                     */
                    function RegisterActionsRequest(properties) {
                        this.actions = [];
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * RegisterActionsRequest actions.
                     * @member {Array.<prodigy.api.v1.IActionSpec>} actions
                     * @memberof prodigy.api.v1.RegisterActionsRequest
                     * @instance
                     */
                    RegisterActionsRequest.prototype.actions = $util.emptyArray;
    
                    /**
                     * Creates a new RegisterActionsRequest instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.RegisterActionsRequest
                     * @static
                     * @param {prodigy.api.v1.IRegisterActionsRequest=} [properties] Properties to set
                     * @returns {prodigy.api.v1.RegisterActionsRequest} RegisterActionsRequest instance
                     */
                    RegisterActionsRequest.create = function create(properties) {
                        return new RegisterActionsRequest(properties);
                    };
    
                    /**
                     * Encodes the specified RegisterActionsRequest message. Does not implicitly {@link prodigy.api.v1.RegisterActionsRequest.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.RegisterActionsRequest
                     * @static
                     * @param {prodigy.api.v1.IRegisterActionsRequest} message RegisterActionsRequest message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    RegisterActionsRequest.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.actions != null && message.actions.length)
                            for (var i = 0; i < message.actions.length; ++i)
                                $root.prodigy.api.v1.ActionSpec.encode(message.actions[i], writer.uint32(/* id 1, wireType 2 =*/10).fork(), q + 1).ldelim();
                        return writer;
                    };
    
                    /**
                     * Decodes a RegisterActionsRequest message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.RegisterActionsRequest
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.RegisterActionsRequest} RegisterActionsRequest
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    RegisterActionsRequest.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.RegisterActionsRequest();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    if (!(message.actions && message.actions.length))
                                        message.actions = [];
                                    message.actions.push($root.prodigy.api.v1.ActionSpec.decode(reader, reader.uint32(), undefined, long + 1));
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a RegisterActionsRequest message.
                     * @function verify
                     * @memberof prodigy.api.v1.RegisterActionsRequest
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    RegisterActionsRequest.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.actions != null && Object.hasOwnProperty.call(message, "actions")) {
                            if (!Array.isArray(message.actions))
                                return "actions: array expected";
                            for (var i = 0; i < message.actions.length; ++i) {
                                var error = $root.prodigy.api.v1.ActionSpec.verify(message.actions[i], long + 1);
                                if (error)
                                    return "actions." + error;
                            }
                        }
                        return null;
                    };
    
                    /**
                     * Creates a RegisterActionsRequest message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.RegisterActionsRequest
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.RegisterActionsRequest} RegisterActionsRequest
                     */
                    RegisterActionsRequest.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.RegisterActionsRequest)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.RegisterActionsRequest: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.RegisterActionsRequest();
                        if (object.actions) {
                            if (!Array.isArray(object.actions))
                                throw TypeError(".prodigy.api.v1.RegisterActionsRequest.actions: array expected");
                            message.actions = [];
                            for (var i = 0; i < object.actions.length; ++i) {
                                if (!$util.isObject(object.actions[i]))
                                    throw TypeError(".prodigy.api.v1.RegisterActionsRequest.actions: object expected");
                                message.actions[i] = $root.prodigy.api.v1.ActionSpec.fromObject(object.actions[i], long + 1);
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a RegisterActionsRequest message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.RegisterActionsRequest
                     * @static
                     * @param {prodigy.api.v1.RegisterActionsRequest} message RegisterActionsRequest
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    RegisterActionsRequest.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.arrays || options.defaults)
                            object.actions = [];
                        if (message.actions && message.actions.length) {
                            object.actions = [];
                            for (var j = 0; j < message.actions.length; ++j)
                                object.actions[j] = $root.prodigy.api.v1.ActionSpec.toObject(message.actions[j], options, q + 1);
                        }
                        return object;
                    };
    
                    /**
                     * Converts this RegisterActionsRequest to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.RegisterActionsRequest
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    RegisterActionsRequest.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for RegisterActionsRequest
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.RegisterActionsRequest
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    RegisterActionsRequest.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.RegisterActionsRequest";
                    };
    
                    return RegisterActionsRequest;
                })();
    
                v1.ActionRegistrationResult = (function() {
    
                    /**
                     * Properties of an ActionRegistrationResult.
                     * @memberof prodigy.api.v1
                     * @interface IActionRegistrationResult
                     * @property {string|null} [id] ActionRegistrationResult id
                     * @property {boolean|null} [accepted] ActionRegistrationResult accepted
                     * @property {string|null} [reason] ActionRegistrationResult reason
                     */
    
                    /**
                     * Constructs a new ActionRegistrationResult.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents an ActionRegistrationResult.
                     * @implements IActionRegistrationResult
                     * @constructor
                     * @param {prodigy.api.v1.IActionRegistrationResult=} [properties] Properties to set
                     */
                    function ActionRegistrationResult(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * ActionRegistrationResult id.
                     * @member {string} id
                     * @memberof prodigy.api.v1.ActionRegistrationResult
                     * @instance
                     */
                    ActionRegistrationResult.prototype.id = "";
    
                    /**
                     * ActionRegistrationResult accepted.
                     * @member {boolean} accepted
                     * @memberof prodigy.api.v1.ActionRegistrationResult
                     * @instance
                     */
                    ActionRegistrationResult.prototype.accepted = false;
    
                    /**
                     * ActionRegistrationResult reason.
                     * @member {string} reason
                     * @memberof prodigy.api.v1.ActionRegistrationResult
                     * @instance
                     */
                    ActionRegistrationResult.prototype.reason = "";
    
                    /**
                     * Creates a new ActionRegistrationResult instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.ActionRegistrationResult
                     * @static
                     * @param {prodigy.api.v1.IActionRegistrationResult=} [properties] Properties to set
                     * @returns {prodigy.api.v1.ActionRegistrationResult} ActionRegistrationResult instance
                     */
                    ActionRegistrationResult.create = function create(properties) {
                        return new ActionRegistrationResult(properties);
                    };
    
                    /**
                     * Encodes the specified ActionRegistrationResult message. Does not implicitly {@link prodigy.api.v1.ActionRegistrationResult.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.ActionRegistrationResult
                     * @static
                     * @param {prodigy.api.v1.IActionRegistrationResult} message ActionRegistrationResult message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    ActionRegistrationResult.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.id != null && Object.hasOwnProperty.call(message, "id"))
                            writer.uint32(/* id 1, wireType 2 =*/10).string(message.id);
                        if (message.accepted != null && Object.hasOwnProperty.call(message, "accepted"))
                            writer.uint32(/* id 2, wireType 0 =*/16).bool(message.accepted);
                        if (message.reason != null && Object.hasOwnProperty.call(message, "reason"))
                            writer.uint32(/* id 3, wireType 2 =*/26).string(message.reason);
                        return writer;
                    };
    
                    /**
                     * Decodes an ActionRegistrationResult message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.ActionRegistrationResult
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.ActionRegistrationResult} ActionRegistrationResult
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    ActionRegistrationResult.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.ActionRegistrationResult();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.id = reader.string();
                                    break;
                                }
                            case 2: {
                                    message.accepted = reader.bool();
                                    break;
                                }
                            case 3: {
                                    message.reason = reader.string();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies an ActionRegistrationResult message.
                     * @function verify
                     * @memberof prodigy.api.v1.ActionRegistrationResult
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    ActionRegistrationResult.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.id != null && Object.hasOwnProperty.call(message, "id"))
                            if (!$util.isString(message.id))
                                return "id: string expected";
                        if (message.accepted != null && Object.hasOwnProperty.call(message, "accepted"))
                            if (typeof message.accepted !== "boolean")
                                return "accepted: boolean expected";
                        if (message.reason != null && Object.hasOwnProperty.call(message, "reason"))
                            if (!$util.isString(message.reason))
                                return "reason: string expected";
                        return null;
                    };
    
                    /**
                     * Creates an ActionRegistrationResult message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.ActionRegistrationResult
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.ActionRegistrationResult} ActionRegistrationResult
                     */
                    ActionRegistrationResult.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.ActionRegistrationResult)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.ActionRegistrationResult: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.ActionRegistrationResult();
                        if (object.id != null)
                            message.id = String(object.id);
                        if (object.accepted != null)
                            message.accepted = Boolean(object.accepted);
                        if (object.reason != null)
                            message.reason = String(object.reason);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from an ActionRegistrationResult message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.ActionRegistrationResult
                     * @static
                     * @param {prodigy.api.v1.ActionRegistrationResult} message ActionRegistrationResult
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    ActionRegistrationResult.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.id = "";
                            object.accepted = false;
                            object.reason = "";
                        }
                        if (message.id != null && Object.hasOwnProperty.call(message, "id"))
                            object.id = message.id;
                        if (message.accepted != null && Object.hasOwnProperty.call(message, "accepted"))
                            object.accepted = message.accepted;
                        if (message.reason != null && Object.hasOwnProperty.call(message, "reason"))
                            object.reason = message.reason;
                        return object;
                    };
    
                    /**
                     * Converts this ActionRegistrationResult to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.ActionRegistrationResult
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    ActionRegistrationResult.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for ActionRegistrationResult
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.ActionRegistrationResult
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    ActionRegistrationResult.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.ActionRegistrationResult";
                    };
    
                    return ActionRegistrationResult;
                })();
    
                v1.RegisterActionsResponse = (function() {
    
                    /**
                     * Properties of a RegisterActionsResponse.
                     * @memberof prodigy.api.v1
                     * @interface IRegisterActionsResponse
                     * @property {Array.<prodigy.api.v1.IActionRegistrationResult>|null} [results] RegisterActionsResponse results
                     */
    
                    /**
                     * Constructs a new RegisterActionsResponse.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a RegisterActionsResponse.
                     * @implements IRegisterActionsResponse
                     * @constructor
                     * @param {prodigy.api.v1.IRegisterActionsResponse=} [properties] Properties to set
                     */
                    function RegisterActionsResponse(properties) {
                        this.results = [];
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * RegisterActionsResponse results.
                     * @member {Array.<prodigy.api.v1.IActionRegistrationResult>} results
                     * @memberof prodigy.api.v1.RegisterActionsResponse
                     * @instance
                     */
                    RegisterActionsResponse.prototype.results = $util.emptyArray;
    
                    /**
                     * Creates a new RegisterActionsResponse instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.RegisterActionsResponse
                     * @static
                     * @param {prodigy.api.v1.IRegisterActionsResponse=} [properties] Properties to set
                     * @returns {prodigy.api.v1.RegisterActionsResponse} RegisterActionsResponse instance
                     */
                    RegisterActionsResponse.create = function create(properties) {
                        return new RegisterActionsResponse(properties);
                    };
    
                    /**
                     * Encodes the specified RegisterActionsResponse message. Does not implicitly {@link prodigy.api.v1.RegisterActionsResponse.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.RegisterActionsResponse
                     * @static
                     * @param {prodigy.api.v1.IRegisterActionsResponse} message RegisterActionsResponse message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    RegisterActionsResponse.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.results != null && message.results.length)
                            for (var i = 0; i < message.results.length; ++i)
                                $root.prodigy.api.v1.ActionRegistrationResult.encode(message.results[i], writer.uint32(/* id 1, wireType 2 =*/10).fork(), q + 1).ldelim();
                        return writer;
                    };
    
                    /**
                     * Decodes a RegisterActionsResponse message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.RegisterActionsResponse
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.RegisterActionsResponse} RegisterActionsResponse
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    RegisterActionsResponse.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.RegisterActionsResponse();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    if (!(message.results && message.results.length))
                                        message.results = [];
                                    message.results.push($root.prodigy.api.v1.ActionRegistrationResult.decode(reader, reader.uint32(), undefined, long + 1));
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a RegisterActionsResponse message.
                     * @function verify
                     * @memberof prodigy.api.v1.RegisterActionsResponse
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    RegisterActionsResponse.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.results != null && Object.hasOwnProperty.call(message, "results")) {
                            if (!Array.isArray(message.results))
                                return "results: array expected";
                            for (var i = 0; i < message.results.length; ++i) {
                                var error = $root.prodigy.api.v1.ActionRegistrationResult.verify(message.results[i], long + 1);
                                if (error)
                                    return "results." + error;
                            }
                        }
                        return null;
                    };
    
                    /**
                     * Creates a RegisterActionsResponse message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.RegisterActionsResponse
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.RegisterActionsResponse} RegisterActionsResponse
                     */
                    RegisterActionsResponse.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.RegisterActionsResponse)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.RegisterActionsResponse: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.RegisterActionsResponse();
                        if (object.results) {
                            if (!Array.isArray(object.results))
                                throw TypeError(".prodigy.api.v1.RegisterActionsResponse.results: array expected");
                            message.results = [];
                            for (var i = 0; i < object.results.length; ++i) {
                                if (!$util.isObject(object.results[i]))
                                    throw TypeError(".prodigy.api.v1.RegisterActionsResponse.results: object expected");
                                message.results[i] = $root.prodigy.api.v1.ActionRegistrationResult.fromObject(object.results[i], long + 1);
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a RegisterActionsResponse message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.RegisterActionsResponse
                     * @static
                     * @param {prodigy.api.v1.RegisterActionsResponse} message RegisterActionsResponse
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    RegisterActionsResponse.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.arrays || options.defaults)
                            object.results = [];
                        if (message.results && message.results.length) {
                            object.results = [];
                            for (var j = 0; j < message.results.length; ++j)
                                object.results[j] = $root.prodigy.api.v1.ActionRegistrationResult.toObject(message.results[j], options, q + 1);
                        }
                        return object;
                    };
    
                    /**
                     * Converts this RegisterActionsResponse to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.RegisterActionsResponse
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    RegisterActionsResponse.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for RegisterActionsResponse
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.RegisterActionsResponse
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    RegisterActionsResponse.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.RegisterActionsResponse";
                    };
    
                    return RegisterActionsResponse;
                })();
    
                v1.UnregisterActionsRequest = (function() {
    
                    /**
                     * Properties of an UnregisterActionsRequest.
                     * @memberof prodigy.api.v1
                     * @interface IUnregisterActionsRequest
                     * @property {Array.<string>|null} [ids] UnregisterActionsRequest ids
                     */
    
                    /**
                     * Constructs a new UnregisterActionsRequest.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents an UnregisterActionsRequest.
                     * @implements IUnregisterActionsRequest
                     * @constructor
                     * @param {prodigy.api.v1.IUnregisterActionsRequest=} [properties] Properties to set
                     */
                    function UnregisterActionsRequest(properties) {
                        this.ids = [];
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * UnregisterActionsRequest ids.
                     * @member {Array.<string>} ids
                     * @memberof prodigy.api.v1.UnregisterActionsRequest
                     * @instance
                     */
                    UnregisterActionsRequest.prototype.ids = $util.emptyArray;
    
                    /**
                     * Creates a new UnregisterActionsRequest instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.UnregisterActionsRequest
                     * @static
                     * @param {prodigy.api.v1.IUnregisterActionsRequest=} [properties] Properties to set
                     * @returns {prodigy.api.v1.UnregisterActionsRequest} UnregisterActionsRequest instance
                     */
                    UnregisterActionsRequest.create = function create(properties) {
                        return new UnregisterActionsRequest(properties);
                    };
    
                    /**
                     * Encodes the specified UnregisterActionsRequest message. Does not implicitly {@link prodigy.api.v1.UnregisterActionsRequest.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.UnregisterActionsRequest
                     * @static
                     * @param {prodigy.api.v1.IUnregisterActionsRequest} message UnregisterActionsRequest message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    UnregisterActionsRequest.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.ids != null && message.ids.length)
                            for (var i = 0; i < message.ids.length; ++i)
                                writer.uint32(/* id 1, wireType 2 =*/10).string(message.ids[i]);
                        return writer;
                    };
    
                    /**
                     * Decodes an UnregisterActionsRequest message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.UnregisterActionsRequest
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.UnregisterActionsRequest} UnregisterActionsRequest
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    UnregisterActionsRequest.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.UnregisterActionsRequest();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    if (!(message.ids && message.ids.length))
                                        message.ids = [];
                                    message.ids.push(reader.string());
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies an UnregisterActionsRequest message.
                     * @function verify
                     * @memberof prodigy.api.v1.UnregisterActionsRequest
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    UnregisterActionsRequest.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.ids != null && Object.hasOwnProperty.call(message, "ids")) {
                            if (!Array.isArray(message.ids))
                                return "ids: array expected";
                            for (var i = 0; i < message.ids.length; ++i)
                                if (!$util.isString(message.ids[i]))
                                    return "ids: string[] expected";
                        }
                        return null;
                    };
    
                    /**
                     * Creates an UnregisterActionsRequest message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.UnregisterActionsRequest
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.UnregisterActionsRequest} UnregisterActionsRequest
                     */
                    UnregisterActionsRequest.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.UnregisterActionsRequest)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.UnregisterActionsRequest: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.UnregisterActionsRequest();
                        if (object.ids) {
                            if (!Array.isArray(object.ids))
                                throw TypeError(".prodigy.api.v1.UnregisterActionsRequest.ids: array expected");
                            message.ids = [];
                            for (var i = 0; i < object.ids.length; ++i)
                                message.ids[i] = String(object.ids[i]);
                        }
                        return message;
                    };
    
                    /**
                     * Creates a plain object from an UnregisterActionsRequest message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.UnregisterActionsRequest
                     * @static
                     * @param {prodigy.api.v1.UnregisterActionsRequest} message UnregisterActionsRequest
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    UnregisterActionsRequest.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.arrays || options.defaults)
                            object.ids = [];
                        if (message.ids && message.ids.length) {
                            object.ids = [];
                            for (var j = 0; j < message.ids.length; ++j)
                                object.ids[j] = message.ids[j];
                        }
                        return object;
                    };
    
                    /**
                     * Converts this UnregisterActionsRequest to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.UnregisterActionsRequest
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    UnregisterActionsRequest.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for UnregisterActionsRequest
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.UnregisterActionsRequest
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    UnregisterActionsRequest.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.UnregisterActionsRequest";
                    };
    
                    return UnregisterActionsRequest;
                })();
    
                v1.ActionInvokedEvent = (function() {
    
                    /**
                     * Properties of an ActionInvokedEvent.
                     * @memberof prodigy.api.v1
                     * @interface IActionInvokedEvent
                     * @property {string|null} [id] ActionInvokedEvent id
                     * @property {string|null} [payloadJson] ActionInvokedEvent payloadJson
                     */
    
                    /**
                     * Constructs a new ActionInvokedEvent.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents an ActionInvokedEvent.
                     * @implements IActionInvokedEvent
                     * @constructor
                     * @param {prodigy.api.v1.IActionInvokedEvent=} [properties] Properties to set
                     */
                    function ActionInvokedEvent(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * ActionInvokedEvent id.
                     * @member {string} id
                     * @memberof prodigy.api.v1.ActionInvokedEvent
                     * @instance
                     */
                    ActionInvokedEvent.prototype.id = "";
    
                    /**
                     * ActionInvokedEvent payloadJson.
                     * @member {string|null|undefined} payloadJson
                     * @memberof prodigy.api.v1.ActionInvokedEvent
                     * @instance
                     */
                    ActionInvokedEvent.prototype.payloadJson = null;
    
                    // OneOf field names bound to virtual getters and setters
                    var $oneOfFields;
    
                    // Virtual OneOf for proto3 optional field
                    Object.defineProperty(ActionInvokedEvent.prototype, "_payloadJson", {
                        get: $util.oneOfGetter($oneOfFields = ["payloadJson"]),
                        set: $util.oneOfSetter($oneOfFields)
                    });
    
                    /**
                     * Creates a new ActionInvokedEvent instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.ActionInvokedEvent
                     * @static
                     * @param {prodigy.api.v1.IActionInvokedEvent=} [properties] Properties to set
                     * @returns {prodigy.api.v1.ActionInvokedEvent} ActionInvokedEvent instance
                     */
                    ActionInvokedEvent.create = function create(properties) {
                        return new ActionInvokedEvent(properties);
                    };
    
                    /**
                     * Encodes the specified ActionInvokedEvent message. Does not implicitly {@link prodigy.api.v1.ActionInvokedEvent.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.ActionInvokedEvent
                     * @static
                     * @param {prodigy.api.v1.IActionInvokedEvent} message ActionInvokedEvent message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    ActionInvokedEvent.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.id != null && Object.hasOwnProperty.call(message, "id"))
                            writer.uint32(/* id 1, wireType 2 =*/10).string(message.id);
                        if (message.payloadJson != null && Object.hasOwnProperty.call(message, "payloadJson"))
                            writer.uint32(/* id 2, wireType 2 =*/18).string(message.payloadJson);
                        return writer;
                    };
    
                    /**
                     * Decodes an ActionInvokedEvent message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.ActionInvokedEvent
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.ActionInvokedEvent} ActionInvokedEvent
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    ActionInvokedEvent.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.ActionInvokedEvent();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.id = reader.string();
                                    break;
                                }
                            case 2: {
                                    message.payloadJson = reader.string();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies an ActionInvokedEvent message.
                     * @function verify
                     * @memberof prodigy.api.v1.ActionInvokedEvent
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    ActionInvokedEvent.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        var properties = {};
                        if (message.id != null && Object.hasOwnProperty.call(message, "id"))
                            if (!$util.isString(message.id))
                                return "id: string expected";
                        if (message.payloadJson != null && Object.hasOwnProperty.call(message, "payloadJson")) {
                            properties._payloadJson = 1;
                            if (!$util.isString(message.payloadJson))
                                return "payloadJson: string expected";
                        }
                        return null;
                    };
    
                    /**
                     * Creates an ActionInvokedEvent message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.ActionInvokedEvent
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.ActionInvokedEvent} ActionInvokedEvent
                     */
                    ActionInvokedEvent.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.ActionInvokedEvent)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.ActionInvokedEvent: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.ActionInvokedEvent();
                        if (object.id != null)
                            message.id = String(object.id);
                        if (object.payloadJson != null)
                            message.payloadJson = String(object.payloadJson);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from an ActionInvokedEvent message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.ActionInvokedEvent
                     * @static
                     * @param {prodigy.api.v1.ActionInvokedEvent} message ActionInvokedEvent
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    ActionInvokedEvent.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults)
                            object.id = "";
                        if (message.id != null && Object.hasOwnProperty.call(message, "id"))
                            object.id = message.id;
                        if (message.payloadJson != null && Object.hasOwnProperty.call(message, "payloadJson")) {
                            object.payloadJson = message.payloadJson;
                            if (options.oneofs)
                                object._payloadJson = "payloadJson";
                        }
                        return object;
                    };
    
                    /**
                     * Converts this ActionInvokedEvent to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.ActionInvokedEvent
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    ActionInvokedEvent.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for ActionInvokedEvent
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.ActionInvokedEvent
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    ActionInvokedEvent.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.ActionInvokedEvent";
                    };
    
                    return ActionInvokedEvent;
                })();
    
                /**
                 * ClientKind enum.
                 * @name prodigy.api.v1.ClientKind
                 * @enum {number}
                 * @property {number} CLIENT_KIND_UNSPECIFIED=0 CLIENT_KIND_UNSPECIFIED value
                 * @property {number} CLIENT_KIND_COMPANION=1 CLIENT_KIND_COMPANION value
                 * @property {number} CLIENT_KIND_WEB_WIDGET=2 CLIENT_KIND_WEB_WIDGET value
                 * @property {number} CLIENT_KIND_THIRD_PARTY=3 CLIENT_KIND_THIRD_PARTY value
                 * @property {number} CLIENT_KIND_DIAGNOSTIC=4 CLIENT_KIND_DIAGNOSTIC value
                 */
                v1.ClientKind = (function() {
                    var valuesById = {}, values = Object.create(valuesById);
                    values[valuesById[0] = "CLIENT_KIND_UNSPECIFIED"] = 0;
                    values[valuesById[1] = "CLIENT_KIND_COMPANION"] = 1;
                    values[valuesById[2] = "CLIENT_KIND_WEB_WIDGET"] = 2;
                    values[valuesById[3] = "CLIENT_KIND_THIRD_PARTY"] = 3;
                    values[valuesById[4] = "CLIENT_KIND_DIAGNOSTIC"] = 4;
                    return values;
                })();
    
                v1.AuthCredentials = (function() {
    
                    /**
                     * Properties of an AuthCredentials.
                     * @memberof prodigy.api.v1
                     * @interface IAuthCredentials
                     * @property {string|null} [clientId] AuthCredentials clientId
                     * @property {boolean|null} [pairingRequest] AuthCredentials pairingRequest
                     */
    
                    /**
                     * Constructs a new AuthCredentials.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents an AuthCredentials.
                     * @implements IAuthCredentials
                     * @constructor
                     * @param {prodigy.api.v1.IAuthCredentials=} [properties] Properties to set
                     */
                    function AuthCredentials(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * AuthCredentials clientId.
                     * @member {string} clientId
                     * @memberof prodigy.api.v1.AuthCredentials
                     * @instance
                     */
                    AuthCredentials.prototype.clientId = "";
    
                    /**
                     * AuthCredentials pairingRequest.
                     * @member {boolean} pairingRequest
                     * @memberof prodigy.api.v1.AuthCredentials
                     * @instance
                     */
                    AuthCredentials.prototype.pairingRequest = false;
    
                    /**
                     * Creates a new AuthCredentials instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.AuthCredentials
                     * @static
                     * @param {prodigy.api.v1.IAuthCredentials=} [properties] Properties to set
                     * @returns {prodigy.api.v1.AuthCredentials} AuthCredentials instance
                     */
                    AuthCredentials.create = function create(properties) {
                        return new AuthCredentials(properties);
                    };
    
                    /**
                     * Encodes the specified AuthCredentials message. Does not implicitly {@link prodigy.api.v1.AuthCredentials.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.AuthCredentials
                     * @static
                     * @param {prodigy.api.v1.IAuthCredentials} message AuthCredentials message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    AuthCredentials.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.clientId != null && Object.hasOwnProperty.call(message, "clientId"))
                            writer.uint32(/* id 1, wireType 2 =*/10).string(message.clientId);
                        if (message.pairingRequest != null && Object.hasOwnProperty.call(message, "pairingRequest"))
                            writer.uint32(/* id 2, wireType 0 =*/16).bool(message.pairingRequest);
                        return writer;
                    };
    
                    /**
                     * Decodes an AuthCredentials message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.AuthCredentials
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.AuthCredentials} AuthCredentials
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    AuthCredentials.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.AuthCredentials();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.clientId = reader.string();
                                    break;
                                }
                            case 2: {
                                    message.pairingRequest = reader.bool();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies an AuthCredentials message.
                     * @function verify
                     * @memberof prodigy.api.v1.AuthCredentials
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    AuthCredentials.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.clientId != null && Object.hasOwnProperty.call(message, "clientId"))
                            if (!$util.isString(message.clientId))
                                return "clientId: string expected";
                        if (message.pairingRequest != null && Object.hasOwnProperty.call(message, "pairingRequest"))
                            if (typeof message.pairingRequest !== "boolean")
                                return "pairingRequest: boolean expected";
                        return null;
                    };
    
                    /**
                     * Creates an AuthCredentials message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.AuthCredentials
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.AuthCredentials} AuthCredentials
                     */
                    AuthCredentials.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.AuthCredentials)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.AuthCredentials: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.AuthCredentials();
                        if (object.clientId != null)
                            message.clientId = String(object.clientId);
                        if (object.pairingRequest != null)
                            message.pairingRequest = Boolean(object.pairingRequest);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from an AuthCredentials message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.AuthCredentials
                     * @static
                     * @param {prodigy.api.v1.AuthCredentials} message AuthCredentials
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    AuthCredentials.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.clientId = "";
                            object.pairingRequest = false;
                        }
                        if (message.clientId != null && Object.hasOwnProperty.call(message, "clientId"))
                            object.clientId = message.clientId;
                        if (message.pairingRequest != null && Object.hasOwnProperty.call(message, "pairingRequest"))
                            object.pairingRequest = message.pairingRequest;
                        return object;
                    };
    
                    /**
                     * Converts this AuthCredentials to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.AuthCredentials
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    AuthCredentials.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for AuthCredentials
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.AuthCredentials
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    AuthCredentials.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.AuthCredentials";
                    };
    
                    return AuthCredentials;
                })();
    
                v1.ClientHello = (function() {
    
                    /**
                     * Properties of a ClientHello.
                     * @memberof prodigy.api.v1
                     * @interface IClientHello
                     * @property {number|null} [requestedApiVersionMajor] ClientHello requestedApiVersionMajor
                     * @property {number|null} [requestedApiVersionMinor] ClientHello requestedApiVersionMinor
                     * @property {string|null} [clientName] ClientHello clientName
                     * @property {prodigy.api.v1.ClientKind|null} [clientKind] ClientHello clientKind
                     * @property {prodigy.api.v1.IAuthCredentials|null} [auth] ClientHello auth
                     */
    
                    /**
                     * Constructs a new ClientHello.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a ClientHello.
                     * @implements IClientHello
                     * @constructor
                     * @param {prodigy.api.v1.IClientHello=} [properties] Properties to set
                     */
                    function ClientHello(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * ClientHello requestedApiVersionMajor.
                     * @member {number} requestedApiVersionMajor
                     * @memberof prodigy.api.v1.ClientHello
                     * @instance
                     */
                    ClientHello.prototype.requestedApiVersionMajor = 0;
    
                    /**
                     * ClientHello requestedApiVersionMinor.
                     * @member {number} requestedApiVersionMinor
                     * @memberof prodigy.api.v1.ClientHello
                     * @instance
                     */
                    ClientHello.prototype.requestedApiVersionMinor = 0;
    
                    /**
                     * ClientHello clientName.
                     * @member {string} clientName
                     * @memberof prodigy.api.v1.ClientHello
                     * @instance
                     */
                    ClientHello.prototype.clientName = "";
    
                    /**
                     * ClientHello clientKind.
                     * @member {prodigy.api.v1.ClientKind} clientKind
                     * @memberof prodigy.api.v1.ClientHello
                     * @instance
                     */
                    ClientHello.prototype.clientKind = 0;
    
                    /**
                     * ClientHello auth.
                     * @member {prodigy.api.v1.IAuthCredentials|null|undefined} auth
                     * @memberof prodigy.api.v1.ClientHello
                     * @instance
                     */
                    ClientHello.prototype.auth = null;
    
                    /**
                     * Creates a new ClientHello instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.ClientHello
                     * @static
                     * @param {prodigy.api.v1.IClientHello=} [properties] Properties to set
                     * @returns {prodigy.api.v1.ClientHello} ClientHello instance
                     */
                    ClientHello.create = function create(properties) {
                        return new ClientHello(properties);
                    };
    
                    /**
                     * Encodes the specified ClientHello message. Does not implicitly {@link prodigy.api.v1.ClientHello.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.ClientHello
                     * @static
                     * @param {prodigy.api.v1.IClientHello} message ClientHello message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    ClientHello.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.requestedApiVersionMajor != null && Object.hasOwnProperty.call(message, "requestedApiVersionMajor"))
                            writer.uint32(/* id 1, wireType 0 =*/8).uint32(message.requestedApiVersionMajor);
                        if (message.requestedApiVersionMinor != null && Object.hasOwnProperty.call(message, "requestedApiVersionMinor"))
                            writer.uint32(/* id 2, wireType 0 =*/16).uint32(message.requestedApiVersionMinor);
                        if (message.clientName != null && Object.hasOwnProperty.call(message, "clientName"))
                            writer.uint32(/* id 3, wireType 2 =*/26).string(message.clientName);
                        if (message.clientKind != null && Object.hasOwnProperty.call(message, "clientKind"))
                            writer.uint32(/* id 4, wireType 0 =*/32).int32(message.clientKind);
                        if (message.auth != null && Object.hasOwnProperty.call(message, "auth"))
                            $root.prodigy.api.v1.AuthCredentials.encode(message.auth, writer.uint32(/* id 5, wireType 2 =*/42).fork(), q + 1).ldelim();
                        return writer;
                    };
    
                    /**
                     * Decodes a ClientHello message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.ClientHello
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.ClientHello} ClientHello
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    ClientHello.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.ClientHello();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.requestedApiVersionMajor = reader.uint32();
                                    break;
                                }
                            case 2: {
                                    message.requestedApiVersionMinor = reader.uint32();
                                    break;
                                }
                            case 3: {
                                    message.clientName = reader.string();
                                    break;
                                }
                            case 4: {
                                    message.clientKind = reader.int32();
                                    break;
                                }
                            case 5: {
                                    message.auth = $root.prodigy.api.v1.AuthCredentials.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a ClientHello message.
                     * @function verify
                     * @memberof prodigy.api.v1.ClientHello
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    ClientHello.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.requestedApiVersionMajor != null && Object.hasOwnProperty.call(message, "requestedApiVersionMajor"))
                            if (!$util.isInteger(message.requestedApiVersionMajor))
                                return "requestedApiVersionMajor: integer expected";
                        if (message.requestedApiVersionMinor != null && Object.hasOwnProperty.call(message, "requestedApiVersionMinor"))
                            if (!$util.isInteger(message.requestedApiVersionMinor))
                                return "requestedApiVersionMinor: integer expected";
                        if (message.clientName != null && Object.hasOwnProperty.call(message, "clientName"))
                            if (!$util.isString(message.clientName))
                                return "clientName: string expected";
                        if (message.clientKind != null && Object.hasOwnProperty.call(message, "clientKind"))
                            switch (message.clientKind) {
                            default:
                                return "clientKind: enum value expected";
                            case 0:
                            case 1:
                            case 2:
                            case 3:
                            case 4:
                                break;
                            }
                        if (message.auth != null && Object.hasOwnProperty.call(message, "auth")) {
                            var error = $root.prodigy.api.v1.AuthCredentials.verify(message.auth, long + 1);
                            if (error)
                                return "auth." + error;
                        }
                        return null;
                    };
    
                    /**
                     * Creates a ClientHello message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.ClientHello
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.ClientHello} ClientHello
                     */
                    ClientHello.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.ClientHello)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.ClientHello: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.ClientHello();
                        if (object.requestedApiVersionMajor != null)
                            message.requestedApiVersionMajor = object.requestedApiVersionMajor >>> 0;
                        if (object.requestedApiVersionMinor != null)
                            message.requestedApiVersionMinor = object.requestedApiVersionMinor >>> 0;
                        if (object.clientName != null)
                            message.clientName = String(object.clientName);
                        switch (object.clientKind) {
                        default:
                            if (typeof object.clientKind === "number") {
                                message.clientKind = object.clientKind;
                                break;
                            }
                            break;
                        case "CLIENT_KIND_UNSPECIFIED":
                        case 0:
                            message.clientKind = 0;
                            break;
                        case "CLIENT_KIND_COMPANION":
                        case 1:
                            message.clientKind = 1;
                            break;
                        case "CLIENT_KIND_WEB_WIDGET":
                        case 2:
                            message.clientKind = 2;
                            break;
                        case "CLIENT_KIND_THIRD_PARTY":
                        case 3:
                            message.clientKind = 3;
                            break;
                        case "CLIENT_KIND_DIAGNOSTIC":
                        case 4:
                            message.clientKind = 4;
                            break;
                        }
                        if (object.auth != null) {
                            if (!$util.isObject(object.auth))
                                throw TypeError(".prodigy.api.v1.ClientHello.auth: object expected");
                            message.auth = $root.prodigy.api.v1.AuthCredentials.fromObject(object.auth, long + 1);
                        }
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a ClientHello message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.ClientHello
                     * @static
                     * @param {prodigy.api.v1.ClientHello} message ClientHello
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    ClientHello.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.requestedApiVersionMajor = 0;
                            object.requestedApiVersionMinor = 0;
                            object.clientName = "";
                            object.clientKind = options.enums === String ? "CLIENT_KIND_UNSPECIFIED" : 0;
                            object.auth = null;
                        }
                        if (message.requestedApiVersionMajor != null && Object.hasOwnProperty.call(message, "requestedApiVersionMajor"))
                            object.requestedApiVersionMajor = message.requestedApiVersionMajor;
                        if (message.requestedApiVersionMinor != null && Object.hasOwnProperty.call(message, "requestedApiVersionMinor"))
                            object.requestedApiVersionMinor = message.requestedApiVersionMinor;
                        if (message.clientName != null && Object.hasOwnProperty.call(message, "clientName"))
                            object.clientName = message.clientName;
                        if (message.clientKind != null && Object.hasOwnProperty.call(message, "clientKind"))
                            object.clientKind = options.enums === String ? $root.prodigy.api.v1.ClientKind[message.clientKind] === undefined ? message.clientKind : $root.prodigy.api.v1.ClientKind[message.clientKind] : message.clientKind;
                        if (message.auth != null && Object.hasOwnProperty.call(message, "auth"))
                            object.auth = $root.prodigy.api.v1.AuthCredentials.toObject(message.auth, options, q + 1);
                        return object;
                    };
    
                    /**
                     * Converts this ClientHello to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.ClientHello
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    ClientHello.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for ClientHello
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.ClientHello
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    ClientHello.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.ClientHello";
                    };
    
                    return ClientHello;
                })();
    
                v1.ServerHello = (function() {
    
                    /**
                     * Properties of a ServerHello.
                     * @memberof prodigy.api.v1
                     * @interface IServerHello
                     * @property {number|null} [apiVersionMajor] ServerHello apiVersionMajor
                     * @property {number|null} [apiVersionMinor] ServerHello apiVersionMinor
                     * @property {string|null} [serverName] ServerHello serverName
                     * @property {string|null} [appVersion] ServerHello appVersion
                     * @property {string|null} [sessionId] ServerHello sessionId
                     * @property {string|null} [grantedClientId] ServerHello grantedClientId
                     * @property {prodigy.api.v1.ICapabilities|null} [capabilities] ServerHello capabilities
                     * @property {string|null} [serverId] ServerHello serverId
                     */
    
                    /**
                     * Constructs a new ServerHello.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a ServerHello.
                     * @implements IServerHello
                     * @constructor
                     * @param {prodigy.api.v1.IServerHello=} [properties] Properties to set
                     */
                    function ServerHello(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * ServerHello apiVersionMajor.
                     * @member {number} apiVersionMajor
                     * @memberof prodigy.api.v1.ServerHello
                     * @instance
                     */
                    ServerHello.prototype.apiVersionMajor = 0;
    
                    /**
                     * ServerHello apiVersionMinor.
                     * @member {number} apiVersionMinor
                     * @memberof prodigy.api.v1.ServerHello
                     * @instance
                     */
                    ServerHello.prototype.apiVersionMinor = 0;
    
                    /**
                     * ServerHello serverName.
                     * @member {string} serverName
                     * @memberof prodigy.api.v1.ServerHello
                     * @instance
                     */
                    ServerHello.prototype.serverName = "";
    
                    /**
                     * ServerHello appVersion.
                     * @member {string} appVersion
                     * @memberof prodigy.api.v1.ServerHello
                     * @instance
                     */
                    ServerHello.prototype.appVersion = "";
    
                    /**
                     * ServerHello sessionId.
                     * @member {string} sessionId
                     * @memberof prodigy.api.v1.ServerHello
                     * @instance
                     */
                    ServerHello.prototype.sessionId = "";
    
                    /**
                     * ServerHello grantedClientId.
                     * @member {string|null|undefined} grantedClientId
                     * @memberof prodigy.api.v1.ServerHello
                     * @instance
                     */
                    ServerHello.prototype.grantedClientId = null;
    
                    /**
                     * ServerHello capabilities.
                     * @member {prodigy.api.v1.ICapabilities|null|undefined} capabilities
                     * @memberof prodigy.api.v1.ServerHello
                     * @instance
                     */
                    ServerHello.prototype.capabilities = null;
    
                    /**
                     * ServerHello serverId.
                     * @member {string|null|undefined} serverId
                     * @memberof prodigy.api.v1.ServerHello
                     * @instance
                     */
                    ServerHello.prototype.serverId = null;
    
                    // OneOf field names bound to virtual getters and setters
                    var $oneOfFields;
    
                    // Virtual OneOf for proto3 optional field
                    Object.defineProperty(ServerHello.prototype, "_grantedClientId", {
                        get: $util.oneOfGetter($oneOfFields = ["grantedClientId"]),
                        set: $util.oneOfSetter($oneOfFields)
                    });
    
                    // Virtual OneOf for proto3 optional field
                    Object.defineProperty(ServerHello.prototype, "_serverId", {
                        get: $util.oneOfGetter($oneOfFields = ["serverId"]),
                        set: $util.oneOfSetter($oneOfFields)
                    });
    
                    /**
                     * Creates a new ServerHello instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.ServerHello
                     * @static
                     * @param {prodigy.api.v1.IServerHello=} [properties] Properties to set
                     * @returns {prodigy.api.v1.ServerHello} ServerHello instance
                     */
                    ServerHello.create = function create(properties) {
                        return new ServerHello(properties);
                    };
    
                    /**
                     * Encodes the specified ServerHello message. Does not implicitly {@link prodigy.api.v1.ServerHello.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.ServerHello
                     * @static
                     * @param {prodigy.api.v1.IServerHello} message ServerHello message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    ServerHello.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.apiVersionMajor != null && Object.hasOwnProperty.call(message, "apiVersionMajor"))
                            writer.uint32(/* id 1, wireType 0 =*/8).uint32(message.apiVersionMajor);
                        if (message.apiVersionMinor != null && Object.hasOwnProperty.call(message, "apiVersionMinor"))
                            writer.uint32(/* id 2, wireType 0 =*/16).uint32(message.apiVersionMinor);
                        if (message.serverName != null && Object.hasOwnProperty.call(message, "serverName"))
                            writer.uint32(/* id 3, wireType 2 =*/26).string(message.serverName);
                        if (message.appVersion != null && Object.hasOwnProperty.call(message, "appVersion"))
                            writer.uint32(/* id 4, wireType 2 =*/34).string(message.appVersion);
                        if (message.sessionId != null && Object.hasOwnProperty.call(message, "sessionId"))
                            writer.uint32(/* id 5, wireType 2 =*/42).string(message.sessionId);
                        if (message.grantedClientId != null && Object.hasOwnProperty.call(message, "grantedClientId"))
                            writer.uint32(/* id 6, wireType 2 =*/50).string(message.grantedClientId);
                        if (message.capabilities != null && Object.hasOwnProperty.call(message, "capabilities"))
                            $root.prodigy.api.v1.Capabilities.encode(message.capabilities, writer.uint32(/* id 7, wireType 2 =*/58).fork(), q + 1).ldelim();
                        if (message.serverId != null && Object.hasOwnProperty.call(message, "serverId"))
                            writer.uint32(/* id 8, wireType 2 =*/66).string(message.serverId);
                        return writer;
                    };
    
                    /**
                     * Decodes a ServerHello message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.ServerHello
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.ServerHello} ServerHello
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    ServerHello.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.ServerHello();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.apiVersionMajor = reader.uint32();
                                    break;
                                }
                            case 2: {
                                    message.apiVersionMinor = reader.uint32();
                                    break;
                                }
                            case 3: {
                                    message.serverName = reader.string();
                                    break;
                                }
                            case 4: {
                                    message.appVersion = reader.string();
                                    break;
                                }
                            case 5: {
                                    message.sessionId = reader.string();
                                    break;
                                }
                            case 6: {
                                    message.grantedClientId = reader.string();
                                    break;
                                }
                            case 7: {
                                    message.capabilities = $root.prodigy.api.v1.Capabilities.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 8: {
                                    message.serverId = reader.string();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a ServerHello message.
                     * @function verify
                     * @memberof prodigy.api.v1.ServerHello
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    ServerHello.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        var properties = {};
                        if (message.apiVersionMajor != null && Object.hasOwnProperty.call(message, "apiVersionMajor"))
                            if (!$util.isInteger(message.apiVersionMajor))
                                return "apiVersionMajor: integer expected";
                        if (message.apiVersionMinor != null && Object.hasOwnProperty.call(message, "apiVersionMinor"))
                            if (!$util.isInteger(message.apiVersionMinor))
                                return "apiVersionMinor: integer expected";
                        if (message.serverName != null && Object.hasOwnProperty.call(message, "serverName"))
                            if (!$util.isString(message.serverName))
                                return "serverName: string expected";
                        if (message.appVersion != null && Object.hasOwnProperty.call(message, "appVersion"))
                            if (!$util.isString(message.appVersion))
                                return "appVersion: string expected";
                        if (message.sessionId != null && Object.hasOwnProperty.call(message, "sessionId"))
                            if (!$util.isString(message.sessionId))
                                return "sessionId: string expected";
                        if (message.grantedClientId != null && Object.hasOwnProperty.call(message, "grantedClientId")) {
                            properties._grantedClientId = 1;
                            if (!$util.isString(message.grantedClientId))
                                return "grantedClientId: string expected";
                        }
                        if (message.capabilities != null && Object.hasOwnProperty.call(message, "capabilities")) {
                            var error = $root.prodigy.api.v1.Capabilities.verify(message.capabilities, long + 1);
                            if (error)
                                return "capabilities." + error;
                        }
                        if (message.serverId != null && Object.hasOwnProperty.call(message, "serverId")) {
                            properties._serverId = 1;
                            if (!$util.isString(message.serverId))
                                return "serverId: string expected";
                        }
                        return null;
                    };
    
                    /**
                     * Creates a ServerHello message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.ServerHello
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.ServerHello} ServerHello
                     */
                    ServerHello.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.ServerHello)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.ServerHello: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.ServerHello();
                        if (object.apiVersionMajor != null)
                            message.apiVersionMajor = object.apiVersionMajor >>> 0;
                        if (object.apiVersionMinor != null)
                            message.apiVersionMinor = object.apiVersionMinor >>> 0;
                        if (object.serverName != null)
                            message.serverName = String(object.serverName);
                        if (object.appVersion != null)
                            message.appVersion = String(object.appVersion);
                        if (object.sessionId != null)
                            message.sessionId = String(object.sessionId);
                        if (object.grantedClientId != null)
                            message.grantedClientId = String(object.grantedClientId);
                        if (object.capabilities != null) {
                            if (!$util.isObject(object.capabilities))
                                throw TypeError(".prodigy.api.v1.ServerHello.capabilities: object expected");
                            message.capabilities = $root.prodigy.api.v1.Capabilities.fromObject(object.capabilities, long + 1);
                        }
                        if (object.serverId != null)
                            message.serverId = String(object.serverId);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a ServerHello message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.ServerHello
                     * @static
                     * @param {prodigy.api.v1.ServerHello} message ServerHello
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    ServerHello.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.apiVersionMajor = 0;
                            object.apiVersionMinor = 0;
                            object.serverName = "";
                            object.appVersion = "";
                            object.sessionId = "";
                            object.capabilities = null;
                        }
                        if (message.apiVersionMajor != null && Object.hasOwnProperty.call(message, "apiVersionMajor"))
                            object.apiVersionMajor = message.apiVersionMajor;
                        if (message.apiVersionMinor != null && Object.hasOwnProperty.call(message, "apiVersionMinor"))
                            object.apiVersionMinor = message.apiVersionMinor;
                        if (message.serverName != null && Object.hasOwnProperty.call(message, "serverName"))
                            object.serverName = message.serverName;
                        if (message.appVersion != null && Object.hasOwnProperty.call(message, "appVersion"))
                            object.appVersion = message.appVersion;
                        if (message.sessionId != null && Object.hasOwnProperty.call(message, "sessionId"))
                            object.sessionId = message.sessionId;
                        if (message.grantedClientId != null && Object.hasOwnProperty.call(message, "grantedClientId")) {
                            object.grantedClientId = message.grantedClientId;
                            if (options.oneofs)
                                object._grantedClientId = "grantedClientId";
                        }
                        if (message.capabilities != null && Object.hasOwnProperty.call(message, "capabilities"))
                            object.capabilities = $root.prodigy.api.v1.Capabilities.toObject(message.capabilities, options, q + 1);
                        if (message.serverId != null && Object.hasOwnProperty.call(message, "serverId")) {
                            object.serverId = message.serverId;
                            if (options.oneofs)
                                object._serverId = "serverId";
                        }
                        return object;
                    };
    
                    /**
                     * Converts this ServerHello to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.ServerHello
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    ServerHello.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for ServerHello
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.ServerHello
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    ServerHello.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.ServerHello";
                    };
    
                    return ServerHello;
                })();
    
                v1.AuthRequired = (function() {
    
                    /**
                     * Properties of an AuthRequired.
                     * @memberof prodigy.api.v1
                     * @interface IAuthRequired
                     * @property {Uint8Array|null} [nonce] AuthRequired nonce
                     */
    
                    /**
                     * Constructs a new AuthRequired.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents an AuthRequired.
                     * @implements IAuthRequired
                     * @constructor
                     * @param {prodigy.api.v1.IAuthRequired=} [properties] Properties to set
                     */
                    function AuthRequired(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * AuthRequired nonce.
                     * @member {Uint8Array} nonce
                     * @memberof prodigy.api.v1.AuthRequired
                     * @instance
                     */
                    AuthRequired.prototype.nonce = $util.newBuffer([]);
    
                    /**
                     * Creates a new AuthRequired instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.AuthRequired
                     * @static
                     * @param {prodigy.api.v1.IAuthRequired=} [properties] Properties to set
                     * @returns {prodigy.api.v1.AuthRequired} AuthRequired instance
                     */
                    AuthRequired.create = function create(properties) {
                        return new AuthRequired(properties);
                    };
    
                    /**
                     * Encodes the specified AuthRequired message. Does not implicitly {@link prodigy.api.v1.AuthRequired.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.AuthRequired
                     * @static
                     * @param {prodigy.api.v1.IAuthRequired} message AuthRequired message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    AuthRequired.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.nonce != null && Object.hasOwnProperty.call(message, "nonce"))
                            writer.uint32(/* id 1, wireType 2 =*/10).bytes(message.nonce);
                        return writer;
                    };
    
                    /**
                     * Decodes an AuthRequired message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.AuthRequired
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.AuthRequired} AuthRequired
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    AuthRequired.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.AuthRequired();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.nonce = reader.bytes();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies an AuthRequired message.
                     * @function verify
                     * @memberof prodigy.api.v1.AuthRequired
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    AuthRequired.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.nonce != null && Object.hasOwnProperty.call(message, "nonce"))
                            if (!(message.nonce && typeof message.nonce.length === "number" || $util.isString(message.nonce)))
                                return "nonce: buffer expected";
                        return null;
                    };
    
                    /**
                     * Creates an AuthRequired message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.AuthRequired
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.AuthRequired} AuthRequired
                     */
                    AuthRequired.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.AuthRequired)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.AuthRequired: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.AuthRequired();
                        if (object.nonce != null)
                            if (typeof object.nonce === "string")
                                $util.base64.decode(object.nonce, message.nonce = $util.newBuffer($util.base64.length(object.nonce)), 0);
                            else if (object.nonce.length >= 0)
                                message.nonce = object.nonce;
                        return message;
                    };
    
                    /**
                     * Creates a plain object from an AuthRequired message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.AuthRequired
                     * @static
                     * @param {prodigy.api.v1.AuthRequired} message AuthRequired
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    AuthRequired.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults)
                            if (options.bytes === String)
                                object.nonce = "";
                            else {
                                object.nonce = [];
                                if (options.bytes !== Array)
                                    object.nonce = $util.newBuffer(object.nonce);
                            }
                        if (message.nonce != null && Object.hasOwnProperty.call(message, "nonce"))
                            object.nonce = options.bytes === String ? $util.base64.encode(message.nonce, 0, message.nonce.length) : options.bytes === Array ? Array.prototype.slice.call(message.nonce) : message.nonce;
                        return object;
                    };
    
                    /**
                     * Converts this AuthRequired to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.AuthRequired
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    AuthRequired.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for AuthRequired
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.AuthRequired
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    AuthRequired.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.AuthRequired";
                    };
    
                    return AuthRequired;
                })();
    
                v1.AuthResponse = (function() {
    
                    /**
                     * Properties of an AuthResponse.
                     * @memberof prodigy.api.v1
                     * @interface IAuthResponse
                     * @property {string|null} [clientId] AuthResponse clientId
                     * @property {Uint8Array|null} [proof] AuthResponse proof
                     */
    
                    /**
                     * Constructs a new AuthResponse.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents an AuthResponse.
                     * @implements IAuthResponse
                     * @constructor
                     * @param {prodigy.api.v1.IAuthResponse=} [properties] Properties to set
                     */
                    function AuthResponse(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * AuthResponse clientId.
                     * @member {string} clientId
                     * @memberof prodigy.api.v1.AuthResponse
                     * @instance
                     */
                    AuthResponse.prototype.clientId = "";
    
                    /**
                     * AuthResponse proof.
                     * @member {Uint8Array} proof
                     * @memberof prodigy.api.v1.AuthResponse
                     * @instance
                     */
                    AuthResponse.prototype.proof = $util.newBuffer([]);
    
                    /**
                     * Creates a new AuthResponse instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.AuthResponse
                     * @static
                     * @param {prodigy.api.v1.IAuthResponse=} [properties] Properties to set
                     * @returns {prodigy.api.v1.AuthResponse} AuthResponse instance
                     */
                    AuthResponse.create = function create(properties) {
                        return new AuthResponse(properties);
                    };
    
                    /**
                     * Encodes the specified AuthResponse message. Does not implicitly {@link prodigy.api.v1.AuthResponse.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.AuthResponse
                     * @static
                     * @param {prodigy.api.v1.IAuthResponse} message AuthResponse message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    AuthResponse.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.clientId != null && Object.hasOwnProperty.call(message, "clientId"))
                            writer.uint32(/* id 1, wireType 2 =*/10).string(message.clientId);
                        if (message.proof != null && Object.hasOwnProperty.call(message, "proof"))
                            writer.uint32(/* id 2, wireType 2 =*/18).bytes(message.proof);
                        return writer;
                    };
    
                    /**
                     * Decodes an AuthResponse message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.AuthResponse
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.AuthResponse} AuthResponse
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    AuthResponse.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.AuthResponse();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.clientId = reader.string();
                                    break;
                                }
                            case 2: {
                                    message.proof = reader.bytes();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies an AuthResponse message.
                     * @function verify
                     * @memberof prodigy.api.v1.AuthResponse
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    AuthResponse.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.clientId != null && Object.hasOwnProperty.call(message, "clientId"))
                            if (!$util.isString(message.clientId))
                                return "clientId: string expected";
                        if (message.proof != null && Object.hasOwnProperty.call(message, "proof"))
                            if (!(message.proof && typeof message.proof.length === "number" || $util.isString(message.proof)))
                                return "proof: buffer expected";
                        return null;
                    };
    
                    /**
                     * Creates an AuthResponse message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.AuthResponse
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.AuthResponse} AuthResponse
                     */
                    AuthResponse.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.AuthResponse)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.AuthResponse: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.AuthResponse();
                        if (object.clientId != null)
                            message.clientId = String(object.clientId);
                        if (object.proof != null)
                            if (typeof object.proof === "string")
                                $util.base64.decode(object.proof, message.proof = $util.newBuffer($util.base64.length(object.proof)), 0);
                            else if (object.proof.length >= 0)
                                message.proof = object.proof;
                        return message;
                    };
    
                    /**
                     * Creates a plain object from an AuthResponse message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.AuthResponse
                     * @static
                     * @param {prodigy.api.v1.AuthResponse} message AuthResponse
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    AuthResponse.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.clientId = "";
                            if (options.bytes === String)
                                object.proof = "";
                            else {
                                object.proof = [];
                                if (options.bytes !== Array)
                                    object.proof = $util.newBuffer(object.proof);
                            }
                        }
                        if (message.clientId != null && Object.hasOwnProperty.call(message, "clientId"))
                            object.clientId = message.clientId;
                        if (message.proof != null && Object.hasOwnProperty.call(message, "proof"))
                            object.proof = options.bytes === String ? $util.base64.encode(message.proof, 0, message.proof.length) : options.bytes === Array ? Array.prototype.slice.call(message.proof) : message.proof;
                        return object;
                    };
    
                    /**
                     * Converts this AuthResponse to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.AuthResponse
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    AuthResponse.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for AuthResponse
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.AuthResponse
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    AuthResponse.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.AuthResponse";
                    };
    
                    return AuthResponse;
                })();
    
                v1.AuthReject = (function() {
    
                    /**
                     * Properties of an AuthReject.
                     * @memberof prodigy.api.v1
                     * @interface IAuthReject
                     * @property {string|null} [reason] AuthReject reason
                     */
    
                    /**
                     * Constructs a new AuthReject.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents an AuthReject.
                     * @implements IAuthReject
                     * @constructor
                     * @param {prodigy.api.v1.IAuthReject=} [properties] Properties to set
                     */
                    function AuthReject(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * AuthReject reason.
                     * @member {string} reason
                     * @memberof prodigy.api.v1.AuthReject
                     * @instance
                     */
                    AuthReject.prototype.reason = "";
    
                    /**
                     * Creates a new AuthReject instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.AuthReject
                     * @static
                     * @param {prodigy.api.v1.IAuthReject=} [properties] Properties to set
                     * @returns {prodigy.api.v1.AuthReject} AuthReject instance
                     */
                    AuthReject.create = function create(properties) {
                        return new AuthReject(properties);
                    };
    
                    /**
                     * Encodes the specified AuthReject message. Does not implicitly {@link prodigy.api.v1.AuthReject.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.AuthReject
                     * @static
                     * @param {prodigy.api.v1.IAuthReject} message AuthReject message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    AuthReject.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.reason != null && Object.hasOwnProperty.call(message, "reason"))
                            writer.uint32(/* id 1, wireType 2 =*/10).string(message.reason);
                        return writer;
                    };
    
                    /**
                     * Decodes an AuthReject message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.AuthReject
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.AuthReject} AuthReject
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    AuthReject.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.AuthReject();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.reason = reader.string();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies an AuthReject message.
                     * @function verify
                     * @memberof prodigy.api.v1.AuthReject
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    AuthReject.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.reason != null && Object.hasOwnProperty.call(message, "reason"))
                            if (!$util.isString(message.reason))
                                return "reason: string expected";
                        return null;
                    };
    
                    /**
                     * Creates an AuthReject message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.AuthReject
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.AuthReject} AuthReject
                     */
                    AuthReject.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.AuthReject)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.AuthReject: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.AuthReject();
                        if (object.reason != null)
                            message.reason = String(object.reason);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from an AuthReject message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.AuthReject
                     * @static
                     * @param {prodigy.api.v1.AuthReject} message AuthReject
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    AuthReject.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults)
                            object.reason = "";
                        if (message.reason != null && Object.hasOwnProperty.call(message, "reason"))
                            object.reason = message.reason;
                        return object;
                    };
    
                    /**
                     * Converts this AuthReject to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.AuthReject
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    AuthReject.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for AuthReject
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.AuthReject
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    AuthReject.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.AuthReject";
                    };
    
                    return AuthReject;
                })();
    
                v1.PairingChallenge = (function() {
    
                    /**
                     * Properties of a PairingChallenge.
                     * @memberof prodigy.api.v1
                     * @interface IPairingChallenge
                     * @property {Uint8Array|null} [nonce] PairingChallenge nonce
                     * @property {Uint8Array|null} [salt] PairingChallenge salt
                     */
    
                    /**
                     * Constructs a new PairingChallenge.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a PairingChallenge.
                     * @implements IPairingChallenge
                     * @constructor
                     * @param {prodigy.api.v1.IPairingChallenge=} [properties] Properties to set
                     */
                    function PairingChallenge(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * PairingChallenge nonce.
                     * @member {Uint8Array} nonce
                     * @memberof prodigy.api.v1.PairingChallenge
                     * @instance
                     */
                    PairingChallenge.prototype.nonce = $util.newBuffer([]);
    
                    /**
                     * PairingChallenge salt.
                     * @member {Uint8Array} salt
                     * @memberof prodigy.api.v1.PairingChallenge
                     * @instance
                     */
                    PairingChallenge.prototype.salt = $util.newBuffer([]);
    
                    /**
                     * Creates a new PairingChallenge instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.PairingChallenge
                     * @static
                     * @param {prodigy.api.v1.IPairingChallenge=} [properties] Properties to set
                     * @returns {prodigy.api.v1.PairingChallenge} PairingChallenge instance
                     */
                    PairingChallenge.create = function create(properties) {
                        return new PairingChallenge(properties);
                    };
    
                    /**
                     * Encodes the specified PairingChallenge message. Does not implicitly {@link prodigy.api.v1.PairingChallenge.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.PairingChallenge
                     * @static
                     * @param {prodigy.api.v1.IPairingChallenge} message PairingChallenge message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    PairingChallenge.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.nonce != null && Object.hasOwnProperty.call(message, "nonce"))
                            writer.uint32(/* id 1, wireType 2 =*/10).bytes(message.nonce);
                        if (message.salt != null && Object.hasOwnProperty.call(message, "salt"))
                            writer.uint32(/* id 2, wireType 2 =*/18).bytes(message.salt);
                        return writer;
                    };
    
                    /**
                     * Decodes a PairingChallenge message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.PairingChallenge
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.PairingChallenge} PairingChallenge
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    PairingChallenge.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.PairingChallenge();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.nonce = reader.bytes();
                                    break;
                                }
                            case 2: {
                                    message.salt = reader.bytes();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a PairingChallenge message.
                     * @function verify
                     * @memberof prodigy.api.v1.PairingChallenge
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    PairingChallenge.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.nonce != null && Object.hasOwnProperty.call(message, "nonce"))
                            if (!(message.nonce && typeof message.nonce.length === "number" || $util.isString(message.nonce)))
                                return "nonce: buffer expected";
                        if (message.salt != null && Object.hasOwnProperty.call(message, "salt"))
                            if (!(message.salt && typeof message.salt.length === "number" || $util.isString(message.salt)))
                                return "salt: buffer expected";
                        return null;
                    };
    
                    /**
                     * Creates a PairingChallenge message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.PairingChallenge
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.PairingChallenge} PairingChallenge
                     */
                    PairingChallenge.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.PairingChallenge)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.PairingChallenge: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.PairingChallenge();
                        if (object.nonce != null)
                            if (typeof object.nonce === "string")
                                $util.base64.decode(object.nonce, message.nonce = $util.newBuffer($util.base64.length(object.nonce)), 0);
                            else if (object.nonce.length >= 0)
                                message.nonce = object.nonce;
                        if (object.salt != null)
                            if (typeof object.salt === "string")
                                $util.base64.decode(object.salt, message.salt = $util.newBuffer($util.base64.length(object.salt)), 0);
                            else if (object.salt.length >= 0)
                                message.salt = object.salt;
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a PairingChallenge message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.PairingChallenge
                     * @static
                     * @param {prodigy.api.v1.PairingChallenge} message PairingChallenge
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    PairingChallenge.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            if (options.bytes === String)
                                object.nonce = "";
                            else {
                                object.nonce = [];
                                if (options.bytes !== Array)
                                    object.nonce = $util.newBuffer(object.nonce);
                            }
                            if (options.bytes === String)
                                object.salt = "";
                            else {
                                object.salt = [];
                                if (options.bytes !== Array)
                                    object.salt = $util.newBuffer(object.salt);
                            }
                        }
                        if (message.nonce != null && Object.hasOwnProperty.call(message, "nonce"))
                            object.nonce = options.bytes === String ? $util.base64.encode(message.nonce, 0, message.nonce.length) : options.bytes === Array ? Array.prototype.slice.call(message.nonce) : message.nonce;
                        if (message.salt != null && Object.hasOwnProperty.call(message, "salt"))
                            object.salt = options.bytes === String ? $util.base64.encode(message.salt, 0, message.salt.length) : options.bytes === Array ? Array.prototype.slice.call(message.salt) : message.salt;
                        return object;
                    };
    
                    /**
                     * Converts this PairingChallenge to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.PairingChallenge
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    PairingChallenge.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for PairingChallenge
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.PairingChallenge
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    PairingChallenge.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.PairingChallenge";
                    };
    
                    return PairingChallenge;
                })();
    
                v1.PairingResponse = (function() {
    
                    /**
                     * Properties of a PairingResponse.
                     * @memberof prodigy.api.v1
                     * @interface IPairingResponse
                     * @property {Uint8Array|null} [proof] PairingResponse proof
                     */
    
                    /**
                     * Constructs a new PairingResponse.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a PairingResponse.
                     * @implements IPairingResponse
                     * @constructor
                     * @param {prodigy.api.v1.IPairingResponse=} [properties] Properties to set
                     */
                    function PairingResponse(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * PairingResponse proof.
                     * @member {Uint8Array} proof
                     * @memberof prodigy.api.v1.PairingResponse
                     * @instance
                     */
                    PairingResponse.prototype.proof = $util.newBuffer([]);
    
                    /**
                     * Creates a new PairingResponse instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.PairingResponse
                     * @static
                     * @param {prodigy.api.v1.IPairingResponse=} [properties] Properties to set
                     * @returns {prodigy.api.v1.PairingResponse} PairingResponse instance
                     */
                    PairingResponse.create = function create(properties) {
                        return new PairingResponse(properties);
                    };
    
                    /**
                     * Encodes the specified PairingResponse message. Does not implicitly {@link prodigy.api.v1.PairingResponse.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.PairingResponse
                     * @static
                     * @param {prodigy.api.v1.IPairingResponse} message PairingResponse message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    PairingResponse.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.proof != null && Object.hasOwnProperty.call(message, "proof"))
                            writer.uint32(/* id 1, wireType 2 =*/10).bytes(message.proof);
                        return writer;
                    };
    
                    /**
                     * Decodes a PairingResponse message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.PairingResponse
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.PairingResponse} PairingResponse
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    PairingResponse.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.PairingResponse();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.proof = reader.bytes();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a PairingResponse message.
                     * @function verify
                     * @memberof prodigy.api.v1.PairingResponse
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    PairingResponse.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.proof != null && Object.hasOwnProperty.call(message, "proof"))
                            if (!(message.proof && typeof message.proof.length === "number" || $util.isString(message.proof)))
                                return "proof: buffer expected";
                        return null;
                    };
    
                    /**
                     * Creates a PairingResponse message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.PairingResponse
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.PairingResponse} PairingResponse
                     */
                    PairingResponse.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.PairingResponse)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.PairingResponse: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.PairingResponse();
                        if (object.proof != null)
                            if (typeof object.proof === "string")
                                $util.base64.decode(object.proof, message.proof = $util.newBuffer($util.base64.length(object.proof)), 0);
                            else if (object.proof.length >= 0)
                                message.proof = object.proof;
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a PairingResponse message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.PairingResponse
                     * @static
                     * @param {prodigy.api.v1.PairingResponse} message PairingResponse
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    PairingResponse.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults)
                            if (options.bytes === String)
                                object.proof = "";
                            else {
                                object.proof = [];
                                if (options.bytes !== Array)
                                    object.proof = $util.newBuffer(object.proof);
                            }
                        if (message.proof != null && Object.hasOwnProperty.call(message, "proof"))
                            object.proof = options.bytes === String ? $util.base64.encode(message.proof, 0, message.proof.length) : options.bytes === Array ? Array.prototype.slice.call(message.proof) : message.proof;
                        return object;
                    };
    
                    /**
                     * Converts this PairingResponse to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.PairingResponse
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    PairingResponse.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for PairingResponse
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.PairingResponse
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    PairingResponse.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.PairingResponse";
                    };
    
                    return PairingResponse;
                })();
    
                v1.Capabilities = (function() {
    
                    /**
                     * Properties of a Capabilities.
                     * @memberof prodigy.api.v1
                     * @interface ICapabilities
                     * @property {Array.<prodigy.api.v1.Topic>|null} [supportedTopics] Capabilities supportedTopics
                     * @property {prodigy.api.v1.IPhoneCapabilities|null} [phone] Capabilities phone
                     */
    
                    /**
                     * Constructs a new Capabilities.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a Capabilities.
                     * @implements ICapabilities
                     * @constructor
                     * @param {prodigy.api.v1.ICapabilities=} [properties] Properties to set
                     */
                    function Capabilities(properties) {
                        this.supportedTopics = [];
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * Capabilities supportedTopics.
                     * @member {Array.<prodigy.api.v1.Topic>} supportedTopics
                     * @memberof prodigy.api.v1.Capabilities
                     * @instance
                     */
                    Capabilities.prototype.supportedTopics = $util.emptyArray;
    
                    /**
                     * Capabilities phone.
                     * @member {prodigy.api.v1.IPhoneCapabilities|null|undefined} phone
                     * @memberof prodigy.api.v1.Capabilities
                     * @instance
                     */
                    Capabilities.prototype.phone = null;
    
                    /**
                     * Creates a new Capabilities instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.Capabilities
                     * @static
                     * @param {prodigy.api.v1.ICapabilities=} [properties] Properties to set
                     * @returns {prodigy.api.v1.Capabilities} Capabilities instance
                     */
                    Capabilities.create = function create(properties) {
                        return new Capabilities(properties);
                    };
    
                    /**
                     * Encodes the specified Capabilities message. Does not implicitly {@link prodigy.api.v1.Capabilities.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.Capabilities
                     * @static
                     * @param {prodigy.api.v1.ICapabilities} message Capabilities message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    Capabilities.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.supportedTopics != null && message.supportedTopics.length) {
                            writer.uint32(/* id 1, wireType 2 =*/10).fork();
                            for (var i = 0; i < message.supportedTopics.length; ++i)
                                writer.int32(message.supportedTopics[i]);
                            writer.ldelim();
                        }
                        if (message.phone != null && Object.hasOwnProperty.call(message, "phone"))
                            $root.prodigy.api.v1.PhoneCapabilities.encode(message.phone, writer.uint32(/* id 2, wireType 2 =*/18).fork(), q + 1).ldelim();
                        return writer;
                    };
    
                    /**
                     * Decodes a Capabilities message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.Capabilities
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.Capabilities} Capabilities
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    Capabilities.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.Capabilities();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    if (!(message.supportedTopics && message.supportedTopics.length))
                                        message.supportedTopics = [];
                                    if ((tag & 7) === 2) {
                                        var end2 = reader.uint32() + reader.pos;
                                        while (reader.pos < end2)
                                            message.supportedTopics.push(reader.int32());
                                    } else
                                        message.supportedTopics.push(reader.int32());
                                    break;
                                }
                            case 2: {
                                    message.phone = $root.prodigy.api.v1.PhoneCapabilities.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a Capabilities message.
                     * @function verify
                     * @memberof prodigy.api.v1.Capabilities
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    Capabilities.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.supportedTopics != null && Object.hasOwnProperty.call(message, "supportedTopics")) {
                            if (!Array.isArray(message.supportedTopics))
                                return "supportedTopics: array expected";
                            for (var i = 0; i < message.supportedTopics.length; ++i)
                                switch (message.supportedTopics[i]) {
                                default:
                                    return "supportedTopics: enum value[] expected";
                                case 0:
                                case 1:
                                case 2:
                                case 3:
                                case 4:
                                case 5:
                                    break;
                                }
                        }
                        if (message.phone != null && Object.hasOwnProperty.call(message, "phone")) {
                            var error = $root.prodigy.api.v1.PhoneCapabilities.verify(message.phone, long + 1);
                            if (error)
                                return "phone." + error;
                        }
                        return null;
                    };
    
                    /**
                     * Creates a Capabilities message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.Capabilities
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.Capabilities} Capabilities
                     */
                    Capabilities.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.Capabilities)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.Capabilities: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.Capabilities();
                        if (object.supportedTopics) {
                            if (!Array.isArray(object.supportedTopics))
                                throw TypeError(".prodigy.api.v1.Capabilities.supportedTopics: array expected");
                            message.supportedTopics = [];
                            for (var i = 0; i < object.supportedTopics.length; ++i)
                                switch (object.supportedTopics[i]) {
                                default:
                                    if (typeof object.supportedTopics[i] === "number") {
                                        message.supportedTopics[i] = object.supportedTopics[i];
                                        break;
                                    }
                                case "TOPIC_UNSPECIFIED":
                                case 0:
                                    message.supportedTopics[i] = 0;
                                    break;
                                case "TOPIC_MEDIA":
                                case 1:
                                    message.supportedTopics[i] = 1;
                                    break;
                                case "TOPIC_NAVIGATION":
                                case 2:
                                    message.supportedTopics[i] = 2;
                                    break;
                                case "TOPIC_PROJECTION":
                                case 3:
                                    message.supportedTopics[i] = 3;
                                    break;
                                case "TOPIC_PHONE":
                                case 4:
                                    message.supportedTopics[i] = 4;
                                    break;
                                case "TOPIC_SYSTEM":
                                case 5:
                                    message.supportedTopics[i] = 5;
                                    break;
                                }
                        }
                        if (object.phone != null) {
                            if (!$util.isObject(object.phone))
                                throw TypeError(".prodigy.api.v1.Capabilities.phone: object expected");
                            message.phone = $root.prodigy.api.v1.PhoneCapabilities.fromObject(object.phone, long + 1);
                        }
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a Capabilities message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.Capabilities
                     * @static
                     * @param {prodigy.api.v1.Capabilities} message Capabilities
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    Capabilities.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.arrays || options.defaults)
                            object.supportedTopics = [];
                        if (options.defaults)
                            object.phone = null;
                        if (message.supportedTopics && message.supportedTopics.length) {
                            object.supportedTopics = [];
                            for (var j = 0; j < message.supportedTopics.length; ++j)
                                object.supportedTopics[j] = options.enums === String ? $root.prodigy.api.v1.Topic[message.supportedTopics[j]] === undefined ? message.supportedTopics[j] : $root.prodigy.api.v1.Topic[message.supportedTopics[j]] : message.supportedTopics[j];
                        }
                        if (message.phone != null && Object.hasOwnProperty.call(message, "phone"))
                            object.phone = $root.prodigy.api.v1.PhoneCapabilities.toObject(message.phone, options, q + 1);
                        return object;
                    };
    
                    /**
                     * Converts this Capabilities to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.Capabilities
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    Capabilities.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for Capabilities
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.Capabilities
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    Capabilities.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.Capabilities";
                    };
    
                    return Capabilities;
                })();
    
                v1.GetCapabilitiesRequest = (function() {
    
                    /**
                     * Properties of a GetCapabilitiesRequest.
                     * @memberof prodigy.api.v1
                     * @interface IGetCapabilitiesRequest
                     */
    
                    /**
                     * Constructs a new GetCapabilitiesRequest.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a GetCapabilitiesRequest.
                     * @implements IGetCapabilitiesRequest
                     * @constructor
                     * @param {prodigy.api.v1.IGetCapabilitiesRequest=} [properties] Properties to set
                     */
                    function GetCapabilitiesRequest(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * Creates a new GetCapabilitiesRequest instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.GetCapabilitiesRequest
                     * @static
                     * @param {prodigy.api.v1.IGetCapabilitiesRequest=} [properties] Properties to set
                     * @returns {prodigy.api.v1.GetCapabilitiesRequest} GetCapabilitiesRequest instance
                     */
                    GetCapabilitiesRequest.create = function create(properties) {
                        return new GetCapabilitiesRequest(properties);
                    };
    
                    /**
                     * Encodes the specified GetCapabilitiesRequest message. Does not implicitly {@link prodigy.api.v1.GetCapabilitiesRequest.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.GetCapabilitiesRequest
                     * @static
                     * @param {prodigy.api.v1.IGetCapabilitiesRequest} message GetCapabilitiesRequest message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    GetCapabilitiesRequest.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        return writer;
                    };
    
                    /**
                     * Decodes a GetCapabilitiesRequest message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.GetCapabilitiesRequest
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.GetCapabilitiesRequest} GetCapabilitiesRequest
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    GetCapabilitiesRequest.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.GetCapabilitiesRequest();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a GetCapabilitiesRequest message.
                     * @function verify
                     * @memberof prodigy.api.v1.GetCapabilitiesRequest
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    GetCapabilitiesRequest.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        return null;
                    };
    
                    /**
                     * Creates a GetCapabilitiesRequest message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.GetCapabilitiesRequest
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.GetCapabilitiesRequest} GetCapabilitiesRequest
                     */
                    GetCapabilitiesRequest.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.GetCapabilitiesRequest)
                            return object;
                        return new $root.prodigy.api.v1.GetCapabilitiesRequest();
                    };
    
                    /**
                     * Creates a plain object from a GetCapabilitiesRequest message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.GetCapabilitiesRequest
                     * @static
                     * @param {prodigy.api.v1.GetCapabilitiesRequest} message GetCapabilitiesRequest
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    GetCapabilitiesRequest.toObject = function toObject() {
                        return {};
                    };
    
                    /**
                     * Converts this GetCapabilitiesRequest to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.GetCapabilitiesRequest
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    GetCapabilitiesRequest.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for GetCapabilitiesRequest
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.GetCapabilitiesRequest
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    GetCapabilitiesRequest.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.GetCapabilitiesRequest";
                    };
    
                    return GetCapabilitiesRequest;
                })();
    
                v1.CapabilitiesResponse = (function() {
    
                    /**
                     * Properties of a CapabilitiesResponse.
                     * @memberof prodigy.api.v1
                     * @interface ICapabilitiesResponse
                     * @property {prodigy.api.v1.ICapabilities|null} [capabilities] CapabilitiesResponse capabilities
                     */
    
                    /**
                     * Constructs a new CapabilitiesResponse.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a CapabilitiesResponse.
                     * @implements ICapabilitiesResponse
                     * @constructor
                     * @param {prodigy.api.v1.ICapabilitiesResponse=} [properties] Properties to set
                     */
                    function CapabilitiesResponse(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * CapabilitiesResponse capabilities.
                     * @member {prodigy.api.v1.ICapabilities|null|undefined} capabilities
                     * @memberof prodigy.api.v1.CapabilitiesResponse
                     * @instance
                     */
                    CapabilitiesResponse.prototype.capabilities = null;
    
                    /**
                     * Creates a new CapabilitiesResponse instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.CapabilitiesResponse
                     * @static
                     * @param {prodigy.api.v1.ICapabilitiesResponse=} [properties] Properties to set
                     * @returns {prodigy.api.v1.CapabilitiesResponse} CapabilitiesResponse instance
                     */
                    CapabilitiesResponse.create = function create(properties) {
                        return new CapabilitiesResponse(properties);
                    };
    
                    /**
                     * Encodes the specified CapabilitiesResponse message. Does not implicitly {@link prodigy.api.v1.CapabilitiesResponse.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.CapabilitiesResponse
                     * @static
                     * @param {prodigy.api.v1.ICapabilitiesResponse} message CapabilitiesResponse message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    CapabilitiesResponse.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.capabilities != null && Object.hasOwnProperty.call(message, "capabilities"))
                            $root.prodigy.api.v1.Capabilities.encode(message.capabilities, writer.uint32(/* id 1, wireType 2 =*/10).fork(), q + 1).ldelim();
                        return writer;
                    };
    
                    /**
                     * Decodes a CapabilitiesResponse message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.CapabilitiesResponse
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.CapabilitiesResponse} CapabilitiesResponse
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    CapabilitiesResponse.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.CapabilitiesResponse();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.capabilities = $root.prodigy.api.v1.Capabilities.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a CapabilitiesResponse message.
                     * @function verify
                     * @memberof prodigy.api.v1.CapabilitiesResponse
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    CapabilitiesResponse.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.capabilities != null && Object.hasOwnProperty.call(message, "capabilities")) {
                            var error = $root.prodigy.api.v1.Capabilities.verify(message.capabilities, long + 1);
                            if (error)
                                return "capabilities." + error;
                        }
                        return null;
                    };
    
                    /**
                     * Creates a CapabilitiesResponse message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.CapabilitiesResponse
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.CapabilitiesResponse} CapabilitiesResponse
                     */
                    CapabilitiesResponse.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.CapabilitiesResponse)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.CapabilitiesResponse: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.CapabilitiesResponse();
                        if (object.capabilities != null) {
                            if (!$util.isObject(object.capabilities))
                                throw TypeError(".prodigy.api.v1.CapabilitiesResponse.capabilities: object expected");
                            message.capabilities = $root.prodigy.api.v1.Capabilities.fromObject(object.capabilities, long + 1);
                        }
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a CapabilitiesResponse message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.CapabilitiesResponse
                     * @static
                     * @param {prodigy.api.v1.CapabilitiesResponse} message CapabilitiesResponse
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    CapabilitiesResponse.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults)
                            object.capabilities = null;
                        if (message.capabilities != null && Object.hasOwnProperty.call(message, "capabilities"))
                            object.capabilities = $root.prodigy.api.v1.Capabilities.toObject(message.capabilities, options, q + 1);
                        return object;
                    };
    
                    /**
                     * Converts this CapabilitiesResponse to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.CapabilitiesResponse
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    CapabilitiesResponse.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for CapabilitiesResponse
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.CapabilitiesResponse
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    CapabilitiesResponse.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.CapabilitiesResponse";
                    };
    
                    return CapabilitiesResponse;
                })();
    
                v1.SubscribeRequest = (function() {
    
                    /**
                     * Properties of a SubscribeRequest.
                     * @memberof prodigy.api.v1
                     * @interface ISubscribeRequest
                     * @property {Array.<prodigy.api.v1.Topic>|null} [topics] SubscribeRequest topics
                     */
    
                    /**
                     * Constructs a new SubscribeRequest.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a SubscribeRequest.
                     * @implements ISubscribeRequest
                     * @constructor
                     * @param {prodigy.api.v1.ISubscribeRequest=} [properties] Properties to set
                     */
                    function SubscribeRequest(properties) {
                        this.topics = [];
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * SubscribeRequest topics.
                     * @member {Array.<prodigy.api.v1.Topic>} topics
                     * @memberof prodigy.api.v1.SubscribeRequest
                     * @instance
                     */
                    SubscribeRequest.prototype.topics = $util.emptyArray;
    
                    /**
                     * Creates a new SubscribeRequest instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.SubscribeRequest
                     * @static
                     * @param {prodigy.api.v1.ISubscribeRequest=} [properties] Properties to set
                     * @returns {prodigy.api.v1.SubscribeRequest} SubscribeRequest instance
                     */
                    SubscribeRequest.create = function create(properties) {
                        return new SubscribeRequest(properties);
                    };
    
                    /**
                     * Encodes the specified SubscribeRequest message. Does not implicitly {@link prodigy.api.v1.SubscribeRequest.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.SubscribeRequest
                     * @static
                     * @param {prodigy.api.v1.ISubscribeRequest} message SubscribeRequest message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    SubscribeRequest.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.topics != null && message.topics.length) {
                            writer.uint32(/* id 1, wireType 2 =*/10).fork();
                            for (var i = 0; i < message.topics.length; ++i)
                                writer.int32(message.topics[i]);
                            writer.ldelim();
                        }
                        return writer;
                    };
    
                    /**
                     * Decodes a SubscribeRequest message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.SubscribeRequest
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.SubscribeRequest} SubscribeRequest
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    SubscribeRequest.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.SubscribeRequest();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    if (!(message.topics && message.topics.length))
                                        message.topics = [];
                                    if ((tag & 7) === 2) {
                                        var end2 = reader.uint32() + reader.pos;
                                        while (reader.pos < end2)
                                            message.topics.push(reader.int32());
                                    } else
                                        message.topics.push(reader.int32());
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a SubscribeRequest message.
                     * @function verify
                     * @memberof prodigy.api.v1.SubscribeRequest
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    SubscribeRequest.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.topics != null && Object.hasOwnProperty.call(message, "topics")) {
                            if (!Array.isArray(message.topics))
                                return "topics: array expected";
                            for (var i = 0; i < message.topics.length; ++i)
                                switch (message.topics[i]) {
                                default:
                                    return "topics: enum value[] expected";
                                case 0:
                                case 1:
                                case 2:
                                case 3:
                                case 4:
                                case 5:
                                    break;
                                }
                        }
                        return null;
                    };
    
                    /**
                     * Creates a SubscribeRequest message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.SubscribeRequest
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.SubscribeRequest} SubscribeRequest
                     */
                    SubscribeRequest.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.SubscribeRequest)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.SubscribeRequest: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.SubscribeRequest();
                        if (object.topics) {
                            if (!Array.isArray(object.topics))
                                throw TypeError(".prodigy.api.v1.SubscribeRequest.topics: array expected");
                            message.topics = [];
                            for (var i = 0; i < object.topics.length; ++i)
                                switch (object.topics[i]) {
                                default:
                                    if (typeof object.topics[i] === "number") {
                                        message.topics[i] = object.topics[i];
                                        break;
                                    }
                                case "TOPIC_UNSPECIFIED":
                                case 0:
                                    message.topics[i] = 0;
                                    break;
                                case "TOPIC_MEDIA":
                                case 1:
                                    message.topics[i] = 1;
                                    break;
                                case "TOPIC_NAVIGATION":
                                case 2:
                                    message.topics[i] = 2;
                                    break;
                                case "TOPIC_PROJECTION":
                                case 3:
                                    message.topics[i] = 3;
                                    break;
                                case "TOPIC_PHONE":
                                case 4:
                                    message.topics[i] = 4;
                                    break;
                                case "TOPIC_SYSTEM":
                                case 5:
                                    message.topics[i] = 5;
                                    break;
                                }
                        }
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a SubscribeRequest message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.SubscribeRequest
                     * @static
                     * @param {prodigy.api.v1.SubscribeRequest} message SubscribeRequest
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    SubscribeRequest.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.arrays || options.defaults)
                            object.topics = [];
                        if (message.topics && message.topics.length) {
                            object.topics = [];
                            for (var j = 0; j < message.topics.length; ++j)
                                object.topics[j] = options.enums === String ? $root.prodigy.api.v1.Topic[message.topics[j]] === undefined ? message.topics[j] : $root.prodigy.api.v1.Topic[message.topics[j]] : message.topics[j];
                        }
                        return object;
                    };
    
                    /**
                     * Converts this SubscribeRequest to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.SubscribeRequest
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    SubscribeRequest.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for SubscribeRequest
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.SubscribeRequest
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    SubscribeRequest.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.SubscribeRequest";
                    };
    
                    return SubscribeRequest;
                })();
    
                v1.TopicSubscriptionResult = (function() {
    
                    /**
                     * Properties of a TopicSubscriptionResult.
                     * @memberof prodigy.api.v1
                     * @interface ITopicSubscriptionResult
                     * @property {prodigy.api.v1.Topic|null} [topic] TopicSubscriptionResult topic
                     * @property {boolean|null} [accepted] TopicSubscriptionResult accepted
                     * @property {string|null} [reason] TopicSubscriptionResult reason
                     */
    
                    /**
                     * Constructs a new TopicSubscriptionResult.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a TopicSubscriptionResult.
                     * @implements ITopicSubscriptionResult
                     * @constructor
                     * @param {prodigy.api.v1.ITopicSubscriptionResult=} [properties] Properties to set
                     */
                    function TopicSubscriptionResult(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * TopicSubscriptionResult topic.
                     * @member {prodigy.api.v1.Topic} topic
                     * @memberof prodigy.api.v1.TopicSubscriptionResult
                     * @instance
                     */
                    TopicSubscriptionResult.prototype.topic = 0;
    
                    /**
                     * TopicSubscriptionResult accepted.
                     * @member {boolean} accepted
                     * @memberof prodigy.api.v1.TopicSubscriptionResult
                     * @instance
                     */
                    TopicSubscriptionResult.prototype.accepted = false;
    
                    /**
                     * TopicSubscriptionResult reason.
                     * @member {string} reason
                     * @memberof prodigy.api.v1.TopicSubscriptionResult
                     * @instance
                     */
                    TopicSubscriptionResult.prototype.reason = "";
    
                    /**
                     * Creates a new TopicSubscriptionResult instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.TopicSubscriptionResult
                     * @static
                     * @param {prodigy.api.v1.ITopicSubscriptionResult=} [properties] Properties to set
                     * @returns {prodigy.api.v1.TopicSubscriptionResult} TopicSubscriptionResult instance
                     */
                    TopicSubscriptionResult.create = function create(properties) {
                        return new TopicSubscriptionResult(properties);
                    };
    
                    /**
                     * Encodes the specified TopicSubscriptionResult message. Does not implicitly {@link prodigy.api.v1.TopicSubscriptionResult.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.TopicSubscriptionResult
                     * @static
                     * @param {prodigy.api.v1.ITopicSubscriptionResult} message TopicSubscriptionResult message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    TopicSubscriptionResult.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.topic != null && Object.hasOwnProperty.call(message, "topic"))
                            writer.uint32(/* id 1, wireType 0 =*/8).int32(message.topic);
                        if (message.accepted != null && Object.hasOwnProperty.call(message, "accepted"))
                            writer.uint32(/* id 2, wireType 0 =*/16).bool(message.accepted);
                        if (message.reason != null && Object.hasOwnProperty.call(message, "reason"))
                            writer.uint32(/* id 3, wireType 2 =*/26).string(message.reason);
                        return writer;
                    };
    
                    /**
                     * Decodes a TopicSubscriptionResult message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.TopicSubscriptionResult
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.TopicSubscriptionResult} TopicSubscriptionResult
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    TopicSubscriptionResult.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.TopicSubscriptionResult();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.topic = reader.int32();
                                    break;
                                }
                            case 2: {
                                    message.accepted = reader.bool();
                                    break;
                                }
                            case 3: {
                                    message.reason = reader.string();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a TopicSubscriptionResult message.
                     * @function verify
                     * @memberof prodigy.api.v1.TopicSubscriptionResult
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    TopicSubscriptionResult.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.topic != null && Object.hasOwnProperty.call(message, "topic"))
                            switch (message.topic) {
                            default:
                                return "topic: enum value expected";
                            case 0:
                            case 1:
                            case 2:
                            case 3:
                            case 4:
                            case 5:
                                break;
                            }
                        if (message.accepted != null && Object.hasOwnProperty.call(message, "accepted"))
                            if (typeof message.accepted !== "boolean")
                                return "accepted: boolean expected";
                        if (message.reason != null && Object.hasOwnProperty.call(message, "reason"))
                            if (!$util.isString(message.reason))
                                return "reason: string expected";
                        return null;
                    };
    
                    /**
                     * Creates a TopicSubscriptionResult message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.TopicSubscriptionResult
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.TopicSubscriptionResult} TopicSubscriptionResult
                     */
                    TopicSubscriptionResult.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.TopicSubscriptionResult)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.TopicSubscriptionResult: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.TopicSubscriptionResult();
                        switch (object.topic) {
                        default:
                            if (typeof object.topic === "number") {
                                message.topic = object.topic;
                                break;
                            }
                            break;
                        case "TOPIC_UNSPECIFIED":
                        case 0:
                            message.topic = 0;
                            break;
                        case "TOPIC_MEDIA":
                        case 1:
                            message.topic = 1;
                            break;
                        case "TOPIC_NAVIGATION":
                        case 2:
                            message.topic = 2;
                            break;
                        case "TOPIC_PROJECTION":
                        case 3:
                            message.topic = 3;
                            break;
                        case "TOPIC_PHONE":
                        case 4:
                            message.topic = 4;
                            break;
                        case "TOPIC_SYSTEM":
                        case 5:
                            message.topic = 5;
                            break;
                        }
                        if (object.accepted != null)
                            message.accepted = Boolean(object.accepted);
                        if (object.reason != null)
                            message.reason = String(object.reason);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a TopicSubscriptionResult message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.TopicSubscriptionResult
                     * @static
                     * @param {prodigy.api.v1.TopicSubscriptionResult} message TopicSubscriptionResult
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    TopicSubscriptionResult.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.topic = options.enums === String ? "TOPIC_UNSPECIFIED" : 0;
                            object.accepted = false;
                            object.reason = "";
                        }
                        if (message.topic != null && Object.hasOwnProperty.call(message, "topic"))
                            object.topic = options.enums === String ? $root.prodigy.api.v1.Topic[message.topic] === undefined ? message.topic : $root.prodigy.api.v1.Topic[message.topic] : message.topic;
                        if (message.accepted != null && Object.hasOwnProperty.call(message, "accepted"))
                            object.accepted = message.accepted;
                        if (message.reason != null && Object.hasOwnProperty.call(message, "reason"))
                            object.reason = message.reason;
                        return object;
                    };
    
                    /**
                     * Converts this TopicSubscriptionResult to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.TopicSubscriptionResult
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    TopicSubscriptionResult.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for TopicSubscriptionResult
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.TopicSubscriptionResult
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    TopicSubscriptionResult.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.TopicSubscriptionResult";
                    };
    
                    return TopicSubscriptionResult;
                })();
    
                v1.SubscribeResponse = (function() {
    
                    /**
                     * Properties of a SubscribeResponse.
                     * @memberof prodigy.api.v1
                     * @interface ISubscribeResponse
                     * @property {Array.<prodigy.api.v1.ITopicSubscriptionResult>|null} [results] SubscribeResponse results
                     */
    
                    /**
                     * Constructs a new SubscribeResponse.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a SubscribeResponse.
                     * @implements ISubscribeResponse
                     * @constructor
                     * @param {prodigy.api.v1.ISubscribeResponse=} [properties] Properties to set
                     */
                    function SubscribeResponse(properties) {
                        this.results = [];
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * SubscribeResponse results.
                     * @member {Array.<prodigy.api.v1.ITopicSubscriptionResult>} results
                     * @memberof prodigy.api.v1.SubscribeResponse
                     * @instance
                     */
                    SubscribeResponse.prototype.results = $util.emptyArray;
    
                    /**
                     * Creates a new SubscribeResponse instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.SubscribeResponse
                     * @static
                     * @param {prodigy.api.v1.ISubscribeResponse=} [properties] Properties to set
                     * @returns {prodigy.api.v1.SubscribeResponse} SubscribeResponse instance
                     */
                    SubscribeResponse.create = function create(properties) {
                        return new SubscribeResponse(properties);
                    };
    
                    /**
                     * Encodes the specified SubscribeResponse message. Does not implicitly {@link prodigy.api.v1.SubscribeResponse.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.SubscribeResponse
                     * @static
                     * @param {prodigy.api.v1.ISubscribeResponse} message SubscribeResponse message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    SubscribeResponse.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.results != null && message.results.length)
                            for (var i = 0; i < message.results.length; ++i)
                                $root.prodigy.api.v1.TopicSubscriptionResult.encode(message.results[i], writer.uint32(/* id 1, wireType 2 =*/10).fork(), q + 1).ldelim();
                        return writer;
                    };
    
                    /**
                     * Decodes a SubscribeResponse message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.SubscribeResponse
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.SubscribeResponse} SubscribeResponse
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    SubscribeResponse.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.SubscribeResponse();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    if (!(message.results && message.results.length))
                                        message.results = [];
                                    message.results.push($root.prodigy.api.v1.TopicSubscriptionResult.decode(reader, reader.uint32(), undefined, long + 1));
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a SubscribeResponse message.
                     * @function verify
                     * @memberof prodigy.api.v1.SubscribeResponse
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    SubscribeResponse.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.results != null && Object.hasOwnProperty.call(message, "results")) {
                            if (!Array.isArray(message.results))
                                return "results: array expected";
                            for (var i = 0; i < message.results.length; ++i) {
                                var error = $root.prodigy.api.v1.TopicSubscriptionResult.verify(message.results[i], long + 1);
                                if (error)
                                    return "results." + error;
                            }
                        }
                        return null;
                    };
    
                    /**
                     * Creates a SubscribeResponse message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.SubscribeResponse
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.SubscribeResponse} SubscribeResponse
                     */
                    SubscribeResponse.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.SubscribeResponse)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.SubscribeResponse: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.SubscribeResponse();
                        if (object.results) {
                            if (!Array.isArray(object.results))
                                throw TypeError(".prodigy.api.v1.SubscribeResponse.results: array expected");
                            message.results = [];
                            for (var i = 0; i < object.results.length; ++i) {
                                if (!$util.isObject(object.results[i]))
                                    throw TypeError(".prodigy.api.v1.SubscribeResponse.results: object expected");
                                message.results[i] = $root.prodigy.api.v1.TopicSubscriptionResult.fromObject(object.results[i], long + 1);
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a SubscribeResponse message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.SubscribeResponse
                     * @static
                     * @param {prodigy.api.v1.SubscribeResponse} message SubscribeResponse
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    SubscribeResponse.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.arrays || options.defaults)
                            object.results = [];
                        if (message.results && message.results.length) {
                            object.results = [];
                            for (var j = 0; j < message.results.length; ++j)
                                object.results[j] = $root.prodigy.api.v1.TopicSubscriptionResult.toObject(message.results[j], options, q + 1);
                        }
                        return object;
                    };
    
                    /**
                     * Converts this SubscribeResponse to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.SubscribeResponse
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    SubscribeResponse.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for SubscribeResponse
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.SubscribeResponse
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    SubscribeResponse.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.SubscribeResponse";
                    };
    
                    return SubscribeResponse;
                })();
    
                v1.UnsubscribeRequest = (function() {
    
                    /**
                     * Properties of an UnsubscribeRequest.
                     * @memberof prodigy.api.v1
                     * @interface IUnsubscribeRequest
                     * @property {Array.<prodigy.api.v1.Topic>|null} [topics] UnsubscribeRequest topics
                     */
    
                    /**
                     * Constructs a new UnsubscribeRequest.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents an UnsubscribeRequest.
                     * @implements IUnsubscribeRequest
                     * @constructor
                     * @param {prodigy.api.v1.IUnsubscribeRequest=} [properties] Properties to set
                     */
                    function UnsubscribeRequest(properties) {
                        this.topics = [];
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * UnsubscribeRequest topics.
                     * @member {Array.<prodigy.api.v1.Topic>} topics
                     * @memberof prodigy.api.v1.UnsubscribeRequest
                     * @instance
                     */
                    UnsubscribeRequest.prototype.topics = $util.emptyArray;
    
                    /**
                     * Creates a new UnsubscribeRequest instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.UnsubscribeRequest
                     * @static
                     * @param {prodigy.api.v1.IUnsubscribeRequest=} [properties] Properties to set
                     * @returns {prodigy.api.v1.UnsubscribeRequest} UnsubscribeRequest instance
                     */
                    UnsubscribeRequest.create = function create(properties) {
                        return new UnsubscribeRequest(properties);
                    };
    
                    /**
                     * Encodes the specified UnsubscribeRequest message. Does not implicitly {@link prodigy.api.v1.UnsubscribeRequest.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.UnsubscribeRequest
                     * @static
                     * @param {prodigy.api.v1.IUnsubscribeRequest} message UnsubscribeRequest message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    UnsubscribeRequest.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.topics != null && message.topics.length) {
                            writer.uint32(/* id 1, wireType 2 =*/10).fork();
                            for (var i = 0; i < message.topics.length; ++i)
                                writer.int32(message.topics[i]);
                            writer.ldelim();
                        }
                        return writer;
                    };
    
                    /**
                     * Decodes an UnsubscribeRequest message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.UnsubscribeRequest
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.UnsubscribeRequest} UnsubscribeRequest
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    UnsubscribeRequest.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.UnsubscribeRequest();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    if (!(message.topics && message.topics.length))
                                        message.topics = [];
                                    if ((tag & 7) === 2) {
                                        var end2 = reader.uint32() + reader.pos;
                                        while (reader.pos < end2)
                                            message.topics.push(reader.int32());
                                    } else
                                        message.topics.push(reader.int32());
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies an UnsubscribeRequest message.
                     * @function verify
                     * @memberof prodigy.api.v1.UnsubscribeRequest
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    UnsubscribeRequest.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.topics != null && Object.hasOwnProperty.call(message, "topics")) {
                            if (!Array.isArray(message.topics))
                                return "topics: array expected";
                            for (var i = 0; i < message.topics.length; ++i)
                                switch (message.topics[i]) {
                                default:
                                    return "topics: enum value[] expected";
                                case 0:
                                case 1:
                                case 2:
                                case 3:
                                case 4:
                                case 5:
                                    break;
                                }
                        }
                        return null;
                    };
    
                    /**
                     * Creates an UnsubscribeRequest message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.UnsubscribeRequest
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.UnsubscribeRequest} UnsubscribeRequest
                     */
                    UnsubscribeRequest.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.UnsubscribeRequest)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.UnsubscribeRequest: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.UnsubscribeRequest();
                        if (object.topics) {
                            if (!Array.isArray(object.topics))
                                throw TypeError(".prodigy.api.v1.UnsubscribeRequest.topics: array expected");
                            message.topics = [];
                            for (var i = 0; i < object.topics.length; ++i)
                                switch (object.topics[i]) {
                                default:
                                    if (typeof object.topics[i] === "number") {
                                        message.topics[i] = object.topics[i];
                                        break;
                                    }
                                case "TOPIC_UNSPECIFIED":
                                case 0:
                                    message.topics[i] = 0;
                                    break;
                                case "TOPIC_MEDIA":
                                case 1:
                                    message.topics[i] = 1;
                                    break;
                                case "TOPIC_NAVIGATION":
                                case 2:
                                    message.topics[i] = 2;
                                    break;
                                case "TOPIC_PROJECTION":
                                case 3:
                                    message.topics[i] = 3;
                                    break;
                                case "TOPIC_PHONE":
                                case 4:
                                    message.topics[i] = 4;
                                    break;
                                case "TOPIC_SYSTEM":
                                case 5:
                                    message.topics[i] = 5;
                                    break;
                                }
                        }
                        return message;
                    };
    
                    /**
                     * Creates a plain object from an UnsubscribeRequest message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.UnsubscribeRequest
                     * @static
                     * @param {prodigy.api.v1.UnsubscribeRequest} message UnsubscribeRequest
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    UnsubscribeRequest.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.arrays || options.defaults)
                            object.topics = [];
                        if (message.topics && message.topics.length) {
                            object.topics = [];
                            for (var j = 0; j < message.topics.length; ++j)
                                object.topics[j] = options.enums === String ? $root.prodigy.api.v1.Topic[message.topics[j]] === undefined ? message.topics[j] : $root.prodigy.api.v1.Topic[message.topics[j]] : message.topics[j];
                        }
                        return object;
                    };
    
                    /**
                     * Converts this UnsubscribeRequest to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.UnsubscribeRequest
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    UnsubscribeRequest.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for UnsubscribeRequest
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.UnsubscribeRequest
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    UnsubscribeRequest.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.UnsubscribeRequest";
                    };
    
                    return UnsubscribeRequest;
                })();
    
                v1.ApiMessage = (function() {
    
                    /**
                     * Properties of an ApiMessage.
                     * @memberof prodigy.api.v1
                     * @interface IApiMessage
                     * @property {number|Long|null} [requestId] ApiMessage requestId
                     * @property {prodigy.api.v1.IError|null} [error] ApiMessage error
                     * @property {prodigy.api.v1.IAck|null} [ack] ApiMessage ack
                     * @property {prodigy.api.v1.IPing|null} [ping] ApiMessage ping
                     * @property {prodigy.api.v1.IPong|null} [pong] ApiMessage pong
                     * @property {prodigy.api.v1.IClientHello|null} [clientHello] ApiMessage clientHello
                     * @property {prodigy.api.v1.IServerHello|null} [serverHello] ApiMessage serverHello
                     * @property {prodigy.api.v1.IAuthRequired|null} [authRequired] ApiMessage authRequired
                     * @property {prodigy.api.v1.IAuthResponse|null} [authResponse] ApiMessage authResponse
                     * @property {prodigy.api.v1.IAuthReject|null} [authReject] ApiMessage authReject
                     * @property {prodigy.api.v1.IPairingChallenge|null} [pairingChallenge] ApiMessage pairingChallenge
                     * @property {prodigy.api.v1.IPairingResponse|null} [pairingResponse] ApiMessage pairingResponse
                     * @property {prodigy.api.v1.ISubscribeRequest|null} [subscribeRequest] ApiMessage subscribeRequest
                     * @property {prodigy.api.v1.ISubscribeResponse|null} [subscribeResponse] ApiMessage subscribeResponse
                     * @property {prodigy.api.v1.IUnsubscribeRequest|null} [unsubscribeRequest] ApiMessage unsubscribeRequest
                     * @property {prodigy.api.v1.IGetCapabilitiesRequest|null} [getCapabilitiesRequest] ApiMessage getCapabilitiesRequest
                     * @property {prodigy.api.v1.ICapabilitiesResponse|null} [capabilitiesResponse] ApiMessage capabilitiesResponse
                     * @property {prodigy.api.v1.IMediaStatus|null} [mediaStatus] ApiMessage mediaStatus
                     * @property {prodigy.api.v1.INavigationStatus|null} [navigationStatus] ApiMessage navigationStatus
                     * @property {prodigy.api.v1.IProjectionStatus|null} [projectionStatus] ApiMessage projectionStatus
                     * @property {prodigy.api.v1.IPhoneStatus|null} [phoneStatus] ApiMessage phoneStatus
                     * @property {prodigy.api.v1.ISystemStatus|null} [systemStatus] ApiMessage systemStatus
                     * @property {prodigy.api.v1.IListActionsRequest|null} [listActionsRequest] ApiMessage listActionsRequest
                     * @property {prodigy.api.v1.IListActionsResponse|null} [listActionsResponse] ApiMessage listActionsResponse
                     * @property {prodigy.api.v1.IDispatchActionRequest|null} [dispatchActionRequest] ApiMessage dispatchActionRequest
                     * @property {prodigy.api.v1.IDispatchActionResponse|null} [dispatchActionResponse] ApiMessage dispatchActionResponse
                     * @property {prodigy.api.v1.IRegisterActionsRequest|null} [registerActionsRequest] ApiMessage registerActionsRequest
                     * @property {prodigy.api.v1.IRegisterActionsResponse|null} [registerActionsResponse] ApiMessage registerActionsResponse
                     * @property {prodigy.api.v1.IUnregisterActionsRequest|null} [unregisterActionsRequest] ApiMessage unregisterActionsRequest
                     * @property {prodigy.api.v1.IActionInvokedEvent|null} [actionInvoked] ApiMessage actionInvoked
                     * @property {prodigy.api.v1.IPostNotificationRequest|null} [postNotificationRequest] ApiMessage postNotificationRequest
                     * @property {prodigy.api.v1.IPostNotificationResponse|null} [postNotificationResponse] ApiMessage postNotificationResponse
                     * @property {prodigy.api.v1.IDismissNotificationRequest|null} [dismissNotificationRequest] ApiMessage dismissNotificationRequest
                     * @property {prodigy.api.v1.IDialRequest|null} [dialRequest] ApiMessage dialRequest
                     * @property {prodigy.api.v1.IAnswerCallRequest|null} [answerCallRequest] ApiMessage answerCallRequest
                     * @property {prodigy.api.v1.IHangupRequest|null} [hangupRequest] ApiMessage hangupRequest
                     * @property {prodigy.api.v1.ISendDtmfRequest|null} [sendDtmfRequest] ApiMessage sendDtmfRequest
                     * @property {prodigy.api.v1.IPhoneCommandResponse|null} [phoneCommandResponse] ApiMessage phoneCommandResponse
                     * @property {prodigy.api.v1.IGpsReport|null} [gpsReport] ApiMessage gpsReport
                     * @property {prodigy.api.v1.IBatteryReport|null} [batteryReport] ApiMessage batteryReport
                     * @property {prodigy.api.v1.IConnectivityReport|null} [connectivityReport] ApiMessage connectivityReport
                     * @property {prodigy.api.v1.ITimeReport|null} [timeReport] ApiMessage timeReport
                     */
    
                    /**
                     * Constructs a new ApiMessage.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents an ApiMessage.
                     * @implements IApiMessage
                     * @constructor
                     * @param {prodigy.api.v1.IApiMessage=} [properties] Properties to set
                     */
                    function ApiMessage(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * ApiMessage requestId.
                     * @member {number|Long} requestId
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.requestId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;
    
                    /**
                     * ApiMessage error.
                     * @member {prodigy.api.v1.IError|null|undefined} error
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.error = null;
    
                    /**
                     * ApiMessage ack.
                     * @member {prodigy.api.v1.IAck|null|undefined} ack
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.ack = null;
    
                    /**
                     * ApiMessage ping.
                     * @member {prodigy.api.v1.IPing|null|undefined} ping
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.ping = null;
    
                    /**
                     * ApiMessage pong.
                     * @member {prodigy.api.v1.IPong|null|undefined} pong
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.pong = null;
    
                    /**
                     * ApiMessage clientHello.
                     * @member {prodigy.api.v1.IClientHello|null|undefined} clientHello
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.clientHello = null;
    
                    /**
                     * ApiMessage serverHello.
                     * @member {prodigy.api.v1.IServerHello|null|undefined} serverHello
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.serverHello = null;
    
                    /**
                     * ApiMessage authRequired.
                     * @member {prodigy.api.v1.IAuthRequired|null|undefined} authRequired
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.authRequired = null;
    
                    /**
                     * ApiMessage authResponse.
                     * @member {prodigy.api.v1.IAuthResponse|null|undefined} authResponse
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.authResponse = null;
    
                    /**
                     * ApiMessage authReject.
                     * @member {prodigy.api.v1.IAuthReject|null|undefined} authReject
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.authReject = null;
    
                    /**
                     * ApiMessage pairingChallenge.
                     * @member {prodigy.api.v1.IPairingChallenge|null|undefined} pairingChallenge
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.pairingChallenge = null;
    
                    /**
                     * ApiMessage pairingResponse.
                     * @member {prodigy.api.v1.IPairingResponse|null|undefined} pairingResponse
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.pairingResponse = null;
    
                    /**
                     * ApiMessage subscribeRequest.
                     * @member {prodigy.api.v1.ISubscribeRequest|null|undefined} subscribeRequest
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.subscribeRequest = null;
    
                    /**
                     * ApiMessage subscribeResponse.
                     * @member {prodigy.api.v1.ISubscribeResponse|null|undefined} subscribeResponse
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.subscribeResponse = null;
    
                    /**
                     * ApiMessage unsubscribeRequest.
                     * @member {prodigy.api.v1.IUnsubscribeRequest|null|undefined} unsubscribeRequest
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.unsubscribeRequest = null;
    
                    /**
                     * ApiMessage getCapabilitiesRequest.
                     * @member {prodigy.api.v1.IGetCapabilitiesRequest|null|undefined} getCapabilitiesRequest
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.getCapabilitiesRequest = null;
    
                    /**
                     * ApiMessage capabilitiesResponse.
                     * @member {prodigy.api.v1.ICapabilitiesResponse|null|undefined} capabilitiesResponse
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.capabilitiesResponse = null;
    
                    /**
                     * ApiMessage mediaStatus.
                     * @member {prodigy.api.v1.IMediaStatus|null|undefined} mediaStatus
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.mediaStatus = null;
    
                    /**
                     * ApiMessage navigationStatus.
                     * @member {prodigy.api.v1.INavigationStatus|null|undefined} navigationStatus
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.navigationStatus = null;
    
                    /**
                     * ApiMessage projectionStatus.
                     * @member {prodigy.api.v1.IProjectionStatus|null|undefined} projectionStatus
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.projectionStatus = null;
    
                    /**
                     * ApiMessage phoneStatus.
                     * @member {prodigy.api.v1.IPhoneStatus|null|undefined} phoneStatus
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.phoneStatus = null;
    
                    /**
                     * ApiMessage systemStatus.
                     * @member {prodigy.api.v1.ISystemStatus|null|undefined} systemStatus
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.systemStatus = null;
    
                    /**
                     * ApiMessage listActionsRequest.
                     * @member {prodigy.api.v1.IListActionsRequest|null|undefined} listActionsRequest
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.listActionsRequest = null;
    
                    /**
                     * ApiMessage listActionsResponse.
                     * @member {prodigy.api.v1.IListActionsResponse|null|undefined} listActionsResponse
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.listActionsResponse = null;
    
                    /**
                     * ApiMessage dispatchActionRequest.
                     * @member {prodigy.api.v1.IDispatchActionRequest|null|undefined} dispatchActionRequest
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.dispatchActionRequest = null;
    
                    /**
                     * ApiMessage dispatchActionResponse.
                     * @member {prodigy.api.v1.IDispatchActionResponse|null|undefined} dispatchActionResponse
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.dispatchActionResponse = null;
    
                    /**
                     * ApiMessage registerActionsRequest.
                     * @member {prodigy.api.v1.IRegisterActionsRequest|null|undefined} registerActionsRequest
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.registerActionsRequest = null;
    
                    /**
                     * ApiMessage registerActionsResponse.
                     * @member {prodigy.api.v1.IRegisterActionsResponse|null|undefined} registerActionsResponse
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.registerActionsResponse = null;
    
                    /**
                     * ApiMessage unregisterActionsRequest.
                     * @member {prodigy.api.v1.IUnregisterActionsRequest|null|undefined} unregisterActionsRequest
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.unregisterActionsRequest = null;
    
                    /**
                     * ApiMessage actionInvoked.
                     * @member {prodigy.api.v1.IActionInvokedEvent|null|undefined} actionInvoked
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.actionInvoked = null;
    
                    /**
                     * ApiMessage postNotificationRequest.
                     * @member {prodigy.api.v1.IPostNotificationRequest|null|undefined} postNotificationRequest
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.postNotificationRequest = null;
    
                    /**
                     * ApiMessage postNotificationResponse.
                     * @member {prodigy.api.v1.IPostNotificationResponse|null|undefined} postNotificationResponse
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.postNotificationResponse = null;
    
                    /**
                     * ApiMessage dismissNotificationRequest.
                     * @member {prodigy.api.v1.IDismissNotificationRequest|null|undefined} dismissNotificationRequest
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.dismissNotificationRequest = null;
    
                    /**
                     * ApiMessage dialRequest.
                     * @member {prodigy.api.v1.IDialRequest|null|undefined} dialRequest
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.dialRequest = null;
    
                    /**
                     * ApiMessage answerCallRequest.
                     * @member {prodigy.api.v1.IAnswerCallRequest|null|undefined} answerCallRequest
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.answerCallRequest = null;
    
                    /**
                     * ApiMessage hangupRequest.
                     * @member {prodigy.api.v1.IHangupRequest|null|undefined} hangupRequest
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.hangupRequest = null;
    
                    /**
                     * ApiMessage sendDtmfRequest.
                     * @member {prodigy.api.v1.ISendDtmfRequest|null|undefined} sendDtmfRequest
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.sendDtmfRequest = null;
    
                    /**
                     * ApiMessage phoneCommandResponse.
                     * @member {prodigy.api.v1.IPhoneCommandResponse|null|undefined} phoneCommandResponse
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.phoneCommandResponse = null;
    
                    /**
                     * ApiMessage gpsReport.
                     * @member {prodigy.api.v1.IGpsReport|null|undefined} gpsReport
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.gpsReport = null;
    
                    /**
                     * ApiMessage batteryReport.
                     * @member {prodigy.api.v1.IBatteryReport|null|undefined} batteryReport
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.batteryReport = null;
    
                    /**
                     * ApiMessage connectivityReport.
                     * @member {prodigy.api.v1.IConnectivityReport|null|undefined} connectivityReport
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.connectivityReport = null;
    
                    /**
                     * ApiMessage timeReport.
                     * @member {prodigy.api.v1.ITimeReport|null|undefined} timeReport
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    ApiMessage.prototype.timeReport = null;
    
                    // OneOf field names bound to virtual getters and setters
                    var $oneOfFields;
    
                    /**
                     * ApiMessage payload.
                     * @member {"error"|"ack"|"ping"|"pong"|"clientHello"|"serverHello"|"authRequired"|"authResponse"|"authReject"|"pairingChallenge"|"pairingResponse"|"subscribeRequest"|"subscribeResponse"|"unsubscribeRequest"|"getCapabilitiesRequest"|"capabilitiesResponse"|"mediaStatus"|"navigationStatus"|"projectionStatus"|"phoneStatus"|"systemStatus"|"listActionsRequest"|"listActionsResponse"|"dispatchActionRequest"|"dispatchActionResponse"|"registerActionsRequest"|"registerActionsResponse"|"unregisterActionsRequest"|"actionInvoked"|"postNotificationRequest"|"postNotificationResponse"|"dismissNotificationRequest"|"dialRequest"|"answerCallRequest"|"hangupRequest"|"sendDtmfRequest"|"phoneCommandResponse"|"gpsReport"|"batteryReport"|"connectivityReport"|"timeReport"|undefined} payload
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     */
                    Object.defineProperty(ApiMessage.prototype, "payload", {
                        get: $util.oneOfGetter($oneOfFields = ["error", "ack", "ping", "pong", "clientHello", "serverHello", "authRequired", "authResponse", "authReject", "pairingChallenge", "pairingResponse", "subscribeRequest", "subscribeResponse", "unsubscribeRequest", "getCapabilitiesRequest", "capabilitiesResponse", "mediaStatus", "navigationStatus", "projectionStatus", "phoneStatus", "systemStatus", "listActionsRequest", "listActionsResponse", "dispatchActionRequest", "dispatchActionResponse", "registerActionsRequest", "registerActionsResponse", "unregisterActionsRequest", "actionInvoked", "postNotificationRequest", "postNotificationResponse", "dismissNotificationRequest", "dialRequest", "answerCallRequest", "hangupRequest", "sendDtmfRequest", "phoneCommandResponse", "gpsReport", "batteryReport", "connectivityReport", "timeReport"]),
                        set: $util.oneOfSetter($oneOfFields)
                    });
    
                    /**
                     * Creates a new ApiMessage instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.ApiMessage
                     * @static
                     * @param {prodigy.api.v1.IApiMessage=} [properties] Properties to set
                     * @returns {prodigy.api.v1.ApiMessage} ApiMessage instance
                     */
                    ApiMessage.create = function create(properties) {
                        return new ApiMessage(properties);
                    };
    
                    /**
                     * Encodes the specified ApiMessage message. Does not implicitly {@link prodigy.api.v1.ApiMessage.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.ApiMessage
                     * @static
                     * @param {prodigy.api.v1.IApiMessage} message ApiMessage message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    ApiMessage.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.requestId != null && Object.hasOwnProperty.call(message, "requestId"))
                            writer.uint32(/* id 1, wireType 0 =*/8).uint64(message.requestId);
                        if (message.error != null && Object.hasOwnProperty.call(message, "error"))
                            $root.prodigy.api.v1.Error.encode(message.error, writer.uint32(/* id 2, wireType 2 =*/18).fork(), q + 1).ldelim();
                        if (message.ack != null && Object.hasOwnProperty.call(message, "ack"))
                            $root.prodigy.api.v1.Ack.encode(message.ack, writer.uint32(/* id 3, wireType 2 =*/26).fork(), q + 1).ldelim();
                        if (message.ping != null && Object.hasOwnProperty.call(message, "ping"))
                            $root.prodigy.api.v1.Ping.encode(message.ping, writer.uint32(/* id 4, wireType 2 =*/34).fork(), q + 1).ldelim();
                        if (message.pong != null && Object.hasOwnProperty.call(message, "pong"))
                            $root.prodigy.api.v1.Pong.encode(message.pong, writer.uint32(/* id 5, wireType 2 =*/42).fork(), q + 1).ldelim();
                        if (message.clientHello != null && Object.hasOwnProperty.call(message, "clientHello"))
                            $root.prodigy.api.v1.ClientHello.encode(message.clientHello, writer.uint32(/* id 10, wireType 2 =*/82).fork(), q + 1).ldelim();
                        if (message.serverHello != null && Object.hasOwnProperty.call(message, "serverHello"))
                            $root.prodigy.api.v1.ServerHello.encode(message.serverHello, writer.uint32(/* id 11, wireType 2 =*/90).fork(), q + 1).ldelim();
                        if (message.authRequired != null && Object.hasOwnProperty.call(message, "authRequired"))
                            $root.prodigy.api.v1.AuthRequired.encode(message.authRequired, writer.uint32(/* id 12, wireType 2 =*/98).fork(), q + 1).ldelim();
                        if (message.authResponse != null && Object.hasOwnProperty.call(message, "authResponse"))
                            $root.prodigy.api.v1.AuthResponse.encode(message.authResponse, writer.uint32(/* id 13, wireType 2 =*/106).fork(), q + 1).ldelim();
                        if (message.authReject != null && Object.hasOwnProperty.call(message, "authReject"))
                            $root.prodigy.api.v1.AuthReject.encode(message.authReject, writer.uint32(/* id 14, wireType 2 =*/114).fork(), q + 1).ldelim();
                        if (message.pairingChallenge != null && Object.hasOwnProperty.call(message, "pairingChallenge"))
                            $root.prodigy.api.v1.PairingChallenge.encode(message.pairingChallenge, writer.uint32(/* id 15, wireType 2 =*/122).fork(), q + 1).ldelim();
                        if (message.pairingResponse != null && Object.hasOwnProperty.call(message, "pairingResponse"))
                            $root.prodigy.api.v1.PairingResponse.encode(message.pairingResponse, writer.uint32(/* id 16, wireType 2 =*/130).fork(), q + 1).ldelim();
                        if (message.subscribeRequest != null && Object.hasOwnProperty.call(message, "subscribeRequest"))
                            $root.prodigy.api.v1.SubscribeRequest.encode(message.subscribeRequest, writer.uint32(/* id 20, wireType 2 =*/162).fork(), q + 1).ldelim();
                        if (message.subscribeResponse != null && Object.hasOwnProperty.call(message, "subscribeResponse"))
                            $root.prodigy.api.v1.SubscribeResponse.encode(message.subscribeResponse, writer.uint32(/* id 21, wireType 2 =*/170).fork(), q + 1).ldelim();
                        if (message.unsubscribeRequest != null && Object.hasOwnProperty.call(message, "unsubscribeRequest"))
                            $root.prodigy.api.v1.UnsubscribeRequest.encode(message.unsubscribeRequest, writer.uint32(/* id 22, wireType 2 =*/178).fork(), q + 1).ldelim();
                        if (message.getCapabilitiesRequest != null && Object.hasOwnProperty.call(message, "getCapabilitiesRequest"))
                            $root.prodigy.api.v1.GetCapabilitiesRequest.encode(message.getCapabilitiesRequest, writer.uint32(/* id 25, wireType 2 =*/202).fork(), q + 1).ldelim();
                        if (message.capabilitiesResponse != null && Object.hasOwnProperty.call(message, "capabilitiesResponse"))
                            $root.prodigy.api.v1.CapabilitiesResponse.encode(message.capabilitiesResponse, writer.uint32(/* id 26, wireType 2 =*/210).fork(), q + 1).ldelim();
                        if (message.mediaStatus != null && Object.hasOwnProperty.call(message, "mediaStatus"))
                            $root.prodigy.api.v1.MediaStatus.encode(message.mediaStatus, writer.uint32(/* id 30, wireType 2 =*/242).fork(), q + 1).ldelim();
                        if (message.navigationStatus != null && Object.hasOwnProperty.call(message, "navigationStatus"))
                            $root.prodigy.api.v1.NavigationStatus.encode(message.navigationStatus, writer.uint32(/* id 31, wireType 2 =*/250).fork(), q + 1).ldelim();
                        if (message.projectionStatus != null && Object.hasOwnProperty.call(message, "projectionStatus"))
                            $root.prodigy.api.v1.ProjectionStatus.encode(message.projectionStatus, writer.uint32(/* id 32, wireType 2 =*/258).fork(), q + 1).ldelim();
                        if (message.phoneStatus != null && Object.hasOwnProperty.call(message, "phoneStatus"))
                            $root.prodigy.api.v1.PhoneStatus.encode(message.phoneStatus, writer.uint32(/* id 33, wireType 2 =*/266).fork(), q + 1).ldelim();
                        if (message.systemStatus != null && Object.hasOwnProperty.call(message, "systemStatus"))
                            $root.prodigy.api.v1.SystemStatus.encode(message.systemStatus, writer.uint32(/* id 34, wireType 2 =*/274).fork(), q + 1).ldelim();
                        if (message.listActionsRequest != null && Object.hasOwnProperty.call(message, "listActionsRequest"))
                            $root.prodigy.api.v1.ListActionsRequest.encode(message.listActionsRequest, writer.uint32(/* id 40, wireType 2 =*/322).fork(), q + 1).ldelim();
                        if (message.listActionsResponse != null && Object.hasOwnProperty.call(message, "listActionsResponse"))
                            $root.prodigy.api.v1.ListActionsResponse.encode(message.listActionsResponse, writer.uint32(/* id 41, wireType 2 =*/330).fork(), q + 1).ldelim();
                        if (message.dispatchActionRequest != null && Object.hasOwnProperty.call(message, "dispatchActionRequest"))
                            $root.prodigy.api.v1.DispatchActionRequest.encode(message.dispatchActionRequest, writer.uint32(/* id 42, wireType 2 =*/338).fork(), q + 1).ldelim();
                        if (message.dispatchActionResponse != null && Object.hasOwnProperty.call(message, "dispatchActionResponse"))
                            $root.prodigy.api.v1.DispatchActionResponse.encode(message.dispatchActionResponse, writer.uint32(/* id 43, wireType 2 =*/346).fork(), q + 1).ldelim();
                        if (message.registerActionsRequest != null && Object.hasOwnProperty.call(message, "registerActionsRequest"))
                            $root.prodigy.api.v1.RegisterActionsRequest.encode(message.registerActionsRequest, writer.uint32(/* id 44, wireType 2 =*/354).fork(), q + 1).ldelim();
                        if (message.registerActionsResponse != null && Object.hasOwnProperty.call(message, "registerActionsResponse"))
                            $root.prodigy.api.v1.RegisterActionsResponse.encode(message.registerActionsResponse, writer.uint32(/* id 45, wireType 2 =*/362).fork(), q + 1).ldelim();
                        if (message.unregisterActionsRequest != null && Object.hasOwnProperty.call(message, "unregisterActionsRequest"))
                            $root.prodigy.api.v1.UnregisterActionsRequest.encode(message.unregisterActionsRequest, writer.uint32(/* id 46, wireType 2 =*/370).fork(), q + 1).ldelim();
                        if (message.actionInvoked != null && Object.hasOwnProperty.call(message, "actionInvoked"))
                            $root.prodigy.api.v1.ActionInvokedEvent.encode(message.actionInvoked, writer.uint32(/* id 47, wireType 2 =*/378).fork(), q + 1).ldelim();
                        if (message.postNotificationRequest != null && Object.hasOwnProperty.call(message, "postNotificationRequest"))
                            $root.prodigy.api.v1.PostNotificationRequest.encode(message.postNotificationRequest, writer.uint32(/* id 50, wireType 2 =*/402).fork(), q + 1).ldelim();
                        if (message.postNotificationResponse != null && Object.hasOwnProperty.call(message, "postNotificationResponse"))
                            $root.prodigy.api.v1.PostNotificationResponse.encode(message.postNotificationResponse, writer.uint32(/* id 51, wireType 2 =*/410).fork(), q + 1).ldelim();
                        if (message.dismissNotificationRequest != null && Object.hasOwnProperty.call(message, "dismissNotificationRequest"))
                            $root.prodigy.api.v1.DismissNotificationRequest.encode(message.dismissNotificationRequest, writer.uint32(/* id 52, wireType 2 =*/418).fork(), q + 1).ldelim();
                        if (message.dialRequest != null && Object.hasOwnProperty.call(message, "dialRequest"))
                            $root.prodigy.api.v1.DialRequest.encode(message.dialRequest, writer.uint32(/* id 60, wireType 2 =*/482).fork(), q + 1).ldelim();
                        if (message.answerCallRequest != null && Object.hasOwnProperty.call(message, "answerCallRequest"))
                            $root.prodigy.api.v1.AnswerCallRequest.encode(message.answerCallRequest, writer.uint32(/* id 61, wireType 2 =*/490).fork(), q + 1).ldelim();
                        if (message.hangupRequest != null && Object.hasOwnProperty.call(message, "hangupRequest"))
                            $root.prodigy.api.v1.HangupRequest.encode(message.hangupRequest, writer.uint32(/* id 62, wireType 2 =*/498).fork(), q + 1).ldelim();
                        if (message.sendDtmfRequest != null && Object.hasOwnProperty.call(message, "sendDtmfRequest"))
                            $root.prodigy.api.v1.SendDtmfRequest.encode(message.sendDtmfRequest, writer.uint32(/* id 63, wireType 2 =*/506).fork(), q + 1).ldelim();
                        if (message.phoneCommandResponse != null && Object.hasOwnProperty.call(message, "phoneCommandResponse"))
                            $root.prodigy.api.v1.PhoneCommandResponse.encode(message.phoneCommandResponse, writer.uint32(/* id 64, wireType 2 =*/514).fork(), q + 1).ldelim();
                        if (message.gpsReport != null && Object.hasOwnProperty.call(message, "gpsReport"))
                            $root.prodigy.api.v1.GpsReport.encode(message.gpsReport, writer.uint32(/* id 70, wireType 2 =*/562).fork(), q + 1).ldelim();
                        if (message.batteryReport != null && Object.hasOwnProperty.call(message, "batteryReport"))
                            $root.prodigy.api.v1.BatteryReport.encode(message.batteryReport, writer.uint32(/* id 71, wireType 2 =*/570).fork(), q + 1).ldelim();
                        if (message.connectivityReport != null && Object.hasOwnProperty.call(message, "connectivityReport"))
                            $root.prodigy.api.v1.ConnectivityReport.encode(message.connectivityReport, writer.uint32(/* id 72, wireType 2 =*/578).fork(), q + 1).ldelim();
                        if (message.timeReport != null && Object.hasOwnProperty.call(message, "timeReport"))
                            $root.prodigy.api.v1.TimeReport.encode(message.timeReport, writer.uint32(/* id 73, wireType 2 =*/586).fork(), q + 1).ldelim();
                        return writer;
                    };
    
                    /**
                     * Decodes an ApiMessage message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.ApiMessage
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.ApiMessage} ApiMessage
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    ApiMessage.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.ApiMessage();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.requestId = reader.uint64();
                                    break;
                                }
                            case 2: {
                                    message.error = $root.prodigy.api.v1.Error.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 3: {
                                    message.ack = $root.prodigy.api.v1.Ack.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 4: {
                                    message.ping = $root.prodigy.api.v1.Ping.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 5: {
                                    message.pong = $root.prodigy.api.v1.Pong.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 10: {
                                    message.clientHello = $root.prodigy.api.v1.ClientHello.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 11: {
                                    message.serverHello = $root.prodigy.api.v1.ServerHello.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 12: {
                                    message.authRequired = $root.prodigy.api.v1.AuthRequired.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 13: {
                                    message.authResponse = $root.prodigy.api.v1.AuthResponse.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 14: {
                                    message.authReject = $root.prodigy.api.v1.AuthReject.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 15: {
                                    message.pairingChallenge = $root.prodigy.api.v1.PairingChallenge.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 16: {
                                    message.pairingResponse = $root.prodigy.api.v1.PairingResponse.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 20: {
                                    message.subscribeRequest = $root.prodigy.api.v1.SubscribeRequest.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 21: {
                                    message.subscribeResponse = $root.prodigy.api.v1.SubscribeResponse.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 22: {
                                    message.unsubscribeRequest = $root.prodigy.api.v1.UnsubscribeRequest.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 25: {
                                    message.getCapabilitiesRequest = $root.prodigy.api.v1.GetCapabilitiesRequest.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 26: {
                                    message.capabilitiesResponse = $root.prodigy.api.v1.CapabilitiesResponse.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 30: {
                                    message.mediaStatus = $root.prodigy.api.v1.MediaStatus.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 31: {
                                    message.navigationStatus = $root.prodigy.api.v1.NavigationStatus.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 32: {
                                    message.projectionStatus = $root.prodigy.api.v1.ProjectionStatus.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 33: {
                                    message.phoneStatus = $root.prodigy.api.v1.PhoneStatus.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 34: {
                                    message.systemStatus = $root.prodigy.api.v1.SystemStatus.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 40: {
                                    message.listActionsRequest = $root.prodigy.api.v1.ListActionsRequest.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 41: {
                                    message.listActionsResponse = $root.prodigy.api.v1.ListActionsResponse.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 42: {
                                    message.dispatchActionRequest = $root.prodigy.api.v1.DispatchActionRequest.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 43: {
                                    message.dispatchActionResponse = $root.prodigy.api.v1.DispatchActionResponse.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 44: {
                                    message.registerActionsRequest = $root.prodigy.api.v1.RegisterActionsRequest.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 45: {
                                    message.registerActionsResponse = $root.prodigy.api.v1.RegisterActionsResponse.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 46: {
                                    message.unregisterActionsRequest = $root.prodigy.api.v1.UnregisterActionsRequest.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 47: {
                                    message.actionInvoked = $root.prodigy.api.v1.ActionInvokedEvent.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 50: {
                                    message.postNotificationRequest = $root.prodigy.api.v1.PostNotificationRequest.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 51: {
                                    message.postNotificationResponse = $root.prodigy.api.v1.PostNotificationResponse.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 52: {
                                    message.dismissNotificationRequest = $root.prodigy.api.v1.DismissNotificationRequest.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 60: {
                                    message.dialRequest = $root.prodigy.api.v1.DialRequest.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 61: {
                                    message.answerCallRequest = $root.prodigy.api.v1.AnswerCallRequest.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 62: {
                                    message.hangupRequest = $root.prodigy.api.v1.HangupRequest.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 63: {
                                    message.sendDtmfRequest = $root.prodigy.api.v1.SendDtmfRequest.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 64: {
                                    message.phoneCommandResponse = $root.prodigy.api.v1.PhoneCommandResponse.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 70: {
                                    message.gpsReport = $root.prodigy.api.v1.GpsReport.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 71: {
                                    message.batteryReport = $root.prodigy.api.v1.BatteryReport.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 72: {
                                    message.connectivityReport = $root.prodigy.api.v1.ConnectivityReport.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 73: {
                                    message.timeReport = $root.prodigy.api.v1.TimeReport.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies an ApiMessage message.
                     * @function verify
                     * @memberof prodigy.api.v1.ApiMessage
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    ApiMessage.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        var properties = {};
                        if (message.requestId != null && Object.hasOwnProperty.call(message, "requestId"))
                            if (!$util.isInteger(message.requestId) && !(message.requestId && $util.isInteger(message.requestId.low) && $util.isInteger(message.requestId.high)))
                                return "requestId: integer|Long expected";
                        if (message.error != null && Object.hasOwnProperty.call(message, "error")) {
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.Error.verify(message.error, long + 1);
                                if (error)
                                    return "error." + error;
                            }
                        }
                        if (message.ack != null && Object.hasOwnProperty.call(message, "ack")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.Ack.verify(message.ack, long + 1);
                                if (error)
                                    return "ack." + error;
                            }
                        }
                        if (message.ping != null && Object.hasOwnProperty.call(message, "ping")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.Ping.verify(message.ping, long + 1);
                                if (error)
                                    return "ping." + error;
                            }
                        }
                        if (message.pong != null && Object.hasOwnProperty.call(message, "pong")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.Pong.verify(message.pong, long + 1);
                                if (error)
                                    return "pong." + error;
                            }
                        }
                        if (message.clientHello != null && Object.hasOwnProperty.call(message, "clientHello")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.ClientHello.verify(message.clientHello, long + 1);
                                if (error)
                                    return "clientHello." + error;
                            }
                        }
                        if (message.serverHello != null && Object.hasOwnProperty.call(message, "serverHello")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.ServerHello.verify(message.serverHello, long + 1);
                                if (error)
                                    return "serverHello." + error;
                            }
                        }
                        if (message.authRequired != null && Object.hasOwnProperty.call(message, "authRequired")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.AuthRequired.verify(message.authRequired, long + 1);
                                if (error)
                                    return "authRequired." + error;
                            }
                        }
                        if (message.authResponse != null && Object.hasOwnProperty.call(message, "authResponse")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.AuthResponse.verify(message.authResponse, long + 1);
                                if (error)
                                    return "authResponse." + error;
                            }
                        }
                        if (message.authReject != null && Object.hasOwnProperty.call(message, "authReject")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.AuthReject.verify(message.authReject, long + 1);
                                if (error)
                                    return "authReject." + error;
                            }
                        }
                        if (message.pairingChallenge != null && Object.hasOwnProperty.call(message, "pairingChallenge")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.PairingChallenge.verify(message.pairingChallenge, long + 1);
                                if (error)
                                    return "pairingChallenge." + error;
                            }
                        }
                        if (message.pairingResponse != null && Object.hasOwnProperty.call(message, "pairingResponse")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.PairingResponse.verify(message.pairingResponse, long + 1);
                                if (error)
                                    return "pairingResponse." + error;
                            }
                        }
                        if (message.subscribeRequest != null && Object.hasOwnProperty.call(message, "subscribeRequest")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.SubscribeRequest.verify(message.subscribeRequest, long + 1);
                                if (error)
                                    return "subscribeRequest." + error;
                            }
                        }
                        if (message.subscribeResponse != null && Object.hasOwnProperty.call(message, "subscribeResponse")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.SubscribeResponse.verify(message.subscribeResponse, long + 1);
                                if (error)
                                    return "subscribeResponse." + error;
                            }
                        }
                        if (message.unsubscribeRequest != null && Object.hasOwnProperty.call(message, "unsubscribeRequest")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.UnsubscribeRequest.verify(message.unsubscribeRequest, long + 1);
                                if (error)
                                    return "unsubscribeRequest." + error;
                            }
                        }
                        if (message.getCapabilitiesRequest != null && Object.hasOwnProperty.call(message, "getCapabilitiesRequest")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.GetCapabilitiesRequest.verify(message.getCapabilitiesRequest, long + 1);
                                if (error)
                                    return "getCapabilitiesRequest." + error;
                            }
                        }
                        if (message.capabilitiesResponse != null && Object.hasOwnProperty.call(message, "capabilitiesResponse")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.CapabilitiesResponse.verify(message.capabilitiesResponse, long + 1);
                                if (error)
                                    return "capabilitiesResponse." + error;
                            }
                        }
                        if (message.mediaStatus != null && Object.hasOwnProperty.call(message, "mediaStatus")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.MediaStatus.verify(message.mediaStatus, long + 1);
                                if (error)
                                    return "mediaStatus." + error;
                            }
                        }
                        if (message.navigationStatus != null && Object.hasOwnProperty.call(message, "navigationStatus")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.NavigationStatus.verify(message.navigationStatus, long + 1);
                                if (error)
                                    return "navigationStatus." + error;
                            }
                        }
                        if (message.projectionStatus != null && Object.hasOwnProperty.call(message, "projectionStatus")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.ProjectionStatus.verify(message.projectionStatus, long + 1);
                                if (error)
                                    return "projectionStatus." + error;
                            }
                        }
                        if (message.phoneStatus != null && Object.hasOwnProperty.call(message, "phoneStatus")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.PhoneStatus.verify(message.phoneStatus, long + 1);
                                if (error)
                                    return "phoneStatus." + error;
                            }
                        }
                        if (message.systemStatus != null && Object.hasOwnProperty.call(message, "systemStatus")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.SystemStatus.verify(message.systemStatus, long + 1);
                                if (error)
                                    return "systemStatus." + error;
                            }
                        }
                        if (message.listActionsRequest != null && Object.hasOwnProperty.call(message, "listActionsRequest")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.ListActionsRequest.verify(message.listActionsRequest, long + 1);
                                if (error)
                                    return "listActionsRequest." + error;
                            }
                        }
                        if (message.listActionsResponse != null && Object.hasOwnProperty.call(message, "listActionsResponse")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.ListActionsResponse.verify(message.listActionsResponse, long + 1);
                                if (error)
                                    return "listActionsResponse." + error;
                            }
                        }
                        if (message.dispatchActionRequest != null && Object.hasOwnProperty.call(message, "dispatchActionRequest")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.DispatchActionRequest.verify(message.dispatchActionRequest, long + 1);
                                if (error)
                                    return "dispatchActionRequest." + error;
                            }
                        }
                        if (message.dispatchActionResponse != null && Object.hasOwnProperty.call(message, "dispatchActionResponse")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.DispatchActionResponse.verify(message.dispatchActionResponse, long + 1);
                                if (error)
                                    return "dispatchActionResponse." + error;
                            }
                        }
                        if (message.registerActionsRequest != null && Object.hasOwnProperty.call(message, "registerActionsRequest")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.RegisterActionsRequest.verify(message.registerActionsRequest, long + 1);
                                if (error)
                                    return "registerActionsRequest." + error;
                            }
                        }
                        if (message.registerActionsResponse != null && Object.hasOwnProperty.call(message, "registerActionsResponse")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.RegisterActionsResponse.verify(message.registerActionsResponse, long + 1);
                                if (error)
                                    return "registerActionsResponse." + error;
                            }
                        }
                        if (message.unregisterActionsRequest != null && Object.hasOwnProperty.call(message, "unregisterActionsRequest")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.UnregisterActionsRequest.verify(message.unregisterActionsRequest, long + 1);
                                if (error)
                                    return "unregisterActionsRequest." + error;
                            }
                        }
                        if (message.actionInvoked != null && Object.hasOwnProperty.call(message, "actionInvoked")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.ActionInvokedEvent.verify(message.actionInvoked, long + 1);
                                if (error)
                                    return "actionInvoked." + error;
                            }
                        }
                        if (message.postNotificationRequest != null && Object.hasOwnProperty.call(message, "postNotificationRequest")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.PostNotificationRequest.verify(message.postNotificationRequest, long + 1);
                                if (error)
                                    return "postNotificationRequest." + error;
                            }
                        }
                        if (message.postNotificationResponse != null && Object.hasOwnProperty.call(message, "postNotificationResponse")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.PostNotificationResponse.verify(message.postNotificationResponse, long + 1);
                                if (error)
                                    return "postNotificationResponse." + error;
                            }
                        }
                        if (message.dismissNotificationRequest != null && Object.hasOwnProperty.call(message, "dismissNotificationRequest")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.DismissNotificationRequest.verify(message.dismissNotificationRequest, long + 1);
                                if (error)
                                    return "dismissNotificationRequest." + error;
                            }
                        }
                        if (message.dialRequest != null && Object.hasOwnProperty.call(message, "dialRequest")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.DialRequest.verify(message.dialRequest, long + 1);
                                if (error)
                                    return "dialRequest." + error;
                            }
                        }
                        if (message.answerCallRequest != null && Object.hasOwnProperty.call(message, "answerCallRequest")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.AnswerCallRequest.verify(message.answerCallRequest, long + 1);
                                if (error)
                                    return "answerCallRequest." + error;
                            }
                        }
                        if (message.hangupRequest != null && Object.hasOwnProperty.call(message, "hangupRequest")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.HangupRequest.verify(message.hangupRequest, long + 1);
                                if (error)
                                    return "hangupRequest." + error;
                            }
                        }
                        if (message.sendDtmfRequest != null && Object.hasOwnProperty.call(message, "sendDtmfRequest")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.SendDtmfRequest.verify(message.sendDtmfRequest, long + 1);
                                if (error)
                                    return "sendDtmfRequest." + error;
                            }
                        }
                        if (message.phoneCommandResponse != null && Object.hasOwnProperty.call(message, "phoneCommandResponse")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.PhoneCommandResponse.verify(message.phoneCommandResponse, long + 1);
                                if (error)
                                    return "phoneCommandResponse." + error;
                            }
                        }
                        if (message.gpsReport != null && Object.hasOwnProperty.call(message, "gpsReport")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.GpsReport.verify(message.gpsReport, long + 1);
                                if (error)
                                    return "gpsReport." + error;
                            }
                        }
                        if (message.batteryReport != null && Object.hasOwnProperty.call(message, "batteryReport")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.BatteryReport.verify(message.batteryReport, long + 1);
                                if (error)
                                    return "batteryReport." + error;
                            }
                        }
                        if (message.connectivityReport != null && Object.hasOwnProperty.call(message, "connectivityReport")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.ConnectivityReport.verify(message.connectivityReport, long + 1);
                                if (error)
                                    return "connectivityReport." + error;
                            }
                        }
                        if (message.timeReport != null && Object.hasOwnProperty.call(message, "timeReport")) {
                            if (properties.payload === 1)
                                return "payload: multiple values";
                            properties.payload = 1;
                            {
                                var error = $root.prodigy.api.v1.TimeReport.verify(message.timeReport, long + 1);
                                if (error)
                                    return "timeReport." + error;
                            }
                        }
                        return null;
                    };
    
                    /**
                     * Creates an ApiMessage message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.ApiMessage
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.ApiMessage} ApiMessage
                     */
                    ApiMessage.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.ApiMessage)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.ApiMessage: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.ApiMessage();
                        if (object.requestId != null)
                            if ($util.Long)
                                message.requestId = $util.Long.fromValue(object.requestId, true);
                            else if (typeof object.requestId === "string")
                                message.requestId = parseInt(object.requestId, 10);
                            else if (typeof object.requestId === "number")
                                message.requestId = object.requestId;
                            else if (typeof object.requestId === "object")
                                message.requestId = new $util.LongBits(object.requestId.low >>> 0, object.requestId.high >>> 0).toNumber(true);
                        if (object.error != null) {
                            if (!$util.isObject(object.error))
                                throw TypeError(".prodigy.api.v1.ApiMessage.error: object expected");
                            message.error = $root.prodigy.api.v1.Error.fromObject(object.error, long + 1);
                        }
                        if (object.ack != null) {
                            if (!$util.isObject(object.ack))
                                throw TypeError(".prodigy.api.v1.ApiMessage.ack: object expected");
                            message.ack = $root.prodigy.api.v1.Ack.fromObject(object.ack, long + 1);
                        }
                        if (object.ping != null) {
                            if (!$util.isObject(object.ping))
                                throw TypeError(".prodigy.api.v1.ApiMessage.ping: object expected");
                            message.ping = $root.prodigy.api.v1.Ping.fromObject(object.ping, long + 1);
                        }
                        if (object.pong != null) {
                            if (!$util.isObject(object.pong))
                                throw TypeError(".prodigy.api.v1.ApiMessage.pong: object expected");
                            message.pong = $root.prodigy.api.v1.Pong.fromObject(object.pong, long + 1);
                        }
                        if (object.clientHello != null) {
                            if (!$util.isObject(object.clientHello))
                                throw TypeError(".prodigy.api.v1.ApiMessage.clientHello: object expected");
                            message.clientHello = $root.prodigy.api.v1.ClientHello.fromObject(object.clientHello, long + 1);
                        }
                        if (object.serverHello != null) {
                            if (!$util.isObject(object.serverHello))
                                throw TypeError(".prodigy.api.v1.ApiMessage.serverHello: object expected");
                            message.serverHello = $root.prodigy.api.v1.ServerHello.fromObject(object.serverHello, long + 1);
                        }
                        if (object.authRequired != null) {
                            if (!$util.isObject(object.authRequired))
                                throw TypeError(".prodigy.api.v1.ApiMessage.authRequired: object expected");
                            message.authRequired = $root.prodigy.api.v1.AuthRequired.fromObject(object.authRequired, long + 1);
                        }
                        if (object.authResponse != null) {
                            if (!$util.isObject(object.authResponse))
                                throw TypeError(".prodigy.api.v1.ApiMessage.authResponse: object expected");
                            message.authResponse = $root.prodigy.api.v1.AuthResponse.fromObject(object.authResponse, long + 1);
                        }
                        if (object.authReject != null) {
                            if (!$util.isObject(object.authReject))
                                throw TypeError(".prodigy.api.v1.ApiMessage.authReject: object expected");
                            message.authReject = $root.prodigy.api.v1.AuthReject.fromObject(object.authReject, long + 1);
                        }
                        if (object.pairingChallenge != null) {
                            if (!$util.isObject(object.pairingChallenge))
                                throw TypeError(".prodigy.api.v1.ApiMessage.pairingChallenge: object expected");
                            message.pairingChallenge = $root.prodigy.api.v1.PairingChallenge.fromObject(object.pairingChallenge, long + 1);
                        }
                        if (object.pairingResponse != null) {
                            if (!$util.isObject(object.pairingResponse))
                                throw TypeError(".prodigy.api.v1.ApiMessage.pairingResponse: object expected");
                            message.pairingResponse = $root.prodigy.api.v1.PairingResponse.fromObject(object.pairingResponse, long + 1);
                        }
                        if (object.subscribeRequest != null) {
                            if (!$util.isObject(object.subscribeRequest))
                                throw TypeError(".prodigy.api.v1.ApiMessage.subscribeRequest: object expected");
                            message.subscribeRequest = $root.prodigy.api.v1.SubscribeRequest.fromObject(object.subscribeRequest, long + 1);
                        }
                        if (object.subscribeResponse != null) {
                            if (!$util.isObject(object.subscribeResponse))
                                throw TypeError(".prodigy.api.v1.ApiMessage.subscribeResponse: object expected");
                            message.subscribeResponse = $root.prodigy.api.v1.SubscribeResponse.fromObject(object.subscribeResponse, long + 1);
                        }
                        if (object.unsubscribeRequest != null) {
                            if (!$util.isObject(object.unsubscribeRequest))
                                throw TypeError(".prodigy.api.v1.ApiMessage.unsubscribeRequest: object expected");
                            message.unsubscribeRequest = $root.prodigy.api.v1.UnsubscribeRequest.fromObject(object.unsubscribeRequest, long + 1);
                        }
                        if (object.getCapabilitiesRequest != null) {
                            if (!$util.isObject(object.getCapabilitiesRequest))
                                throw TypeError(".prodigy.api.v1.ApiMessage.getCapabilitiesRequest: object expected");
                            message.getCapabilitiesRequest = $root.prodigy.api.v1.GetCapabilitiesRequest.fromObject(object.getCapabilitiesRequest, long + 1);
                        }
                        if (object.capabilitiesResponse != null) {
                            if (!$util.isObject(object.capabilitiesResponse))
                                throw TypeError(".prodigy.api.v1.ApiMessage.capabilitiesResponse: object expected");
                            message.capabilitiesResponse = $root.prodigy.api.v1.CapabilitiesResponse.fromObject(object.capabilitiesResponse, long + 1);
                        }
                        if (object.mediaStatus != null) {
                            if (!$util.isObject(object.mediaStatus))
                                throw TypeError(".prodigy.api.v1.ApiMessage.mediaStatus: object expected");
                            message.mediaStatus = $root.prodigy.api.v1.MediaStatus.fromObject(object.mediaStatus, long + 1);
                        }
                        if (object.navigationStatus != null) {
                            if (!$util.isObject(object.navigationStatus))
                                throw TypeError(".prodigy.api.v1.ApiMessage.navigationStatus: object expected");
                            message.navigationStatus = $root.prodigy.api.v1.NavigationStatus.fromObject(object.navigationStatus, long + 1);
                        }
                        if (object.projectionStatus != null) {
                            if (!$util.isObject(object.projectionStatus))
                                throw TypeError(".prodigy.api.v1.ApiMessage.projectionStatus: object expected");
                            message.projectionStatus = $root.prodigy.api.v1.ProjectionStatus.fromObject(object.projectionStatus, long + 1);
                        }
                        if (object.phoneStatus != null) {
                            if (!$util.isObject(object.phoneStatus))
                                throw TypeError(".prodigy.api.v1.ApiMessage.phoneStatus: object expected");
                            message.phoneStatus = $root.prodigy.api.v1.PhoneStatus.fromObject(object.phoneStatus, long + 1);
                        }
                        if (object.systemStatus != null) {
                            if (!$util.isObject(object.systemStatus))
                                throw TypeError(".prodigy.api.v1.ApiMessage.systemStatus: object expected");
                            message.systemStatus = $root.prodigy.api.v1.SystemStatus.fromObject(object.systemStatus, long + 1);
                        }
                        if (object.listActionsRequest != null) {
                            if (!$util.isObject(object.listActionsRequest))
                                throw TypeError(".prodigy.api.v1.ApiMessage.listActionsRequest: object expected");
                            message.listActionsRequest = $root.prodigy.api.v1.ListActionsRequest.fromObject(object.listActionsRequest, long + 1);
                        }
                        if (object.listActionsResponse != null) {
                            if (!$util.isObject(object.listActionsResponse))
                                throw TypeError(".prodigy.api.v1.ApiMessage.listActionsResponse: object expected");
                            message.listActionsResponse = $root.prodigy.api.v1.ListActionsResponse.fromObject(object.listActionsResponse, long + 1);
                        }
                        if (object.dispatchActionRequest != null) {
                            if (!$util.isObject(object.dispatchActionRequest))
                                throw TypeError(".prodigy.api.v1.ApiMessage.dispatchActionRequest: object expected");
                            message.dispatchActionRequest = $root.prodigy.api.v1.DispatchActionRequest.fromObject(object.dispatchActionRequest, long + 1);
                        }
                        if (object.dispatchActionResponse != null) {
                            if (!$util.isObject(object.dispatchActionResponse))
                                throw TypeError(".prodigy.api.v1.ApiMessage.dispatchActionResponse: object expected");
                            message.dispatchActionResponse = $root.prodigy.api.v1.DispatchActionResponse.fromObject(object.dispatchActionResponse, long + 1);
                        }
                        if (object.registerActionsRequest != null) {
                            if (!$util.isObject(object.registerActionsRequest))
                                throw TypeError(".prodigy.api.v1.ApiMessage.registerActionsRequest: object expected");
                            message.registerActionsRequest = $root.prodigy.api.v1.RegisterActionsRequest.fromObject(object.registerActionsRequest, long + 1);
                        }
                        if (object.registerActionsResponse != null) {
                            if (!$util.isObject(object.registerActionsResponse))
                                throw TypeError(".prodigy.api.v1.ApiMessage.registerActionsResponse: object expected");
                            message.registerActionsResponse = $root.prodigy.api.v1.RegisterActionsResponse.fromObject(object.registerActionsResponse, long + 1);
                        }
                        if (object.unregisterActionsRequest != null) {
                            if (!$util.isObject(object.unregisterActionsRequest))
                                throw TypeError(".prodigy.api.v1.ApiMessage.unregisterActionsRequest: object expected");
                            message.unregisterActionsRequest = $root.prodigy.api.v1.UnregisterActionsRequest.fromObject(object.unregisterActionsRequest, long + 1);
                        }
                        if (object.actionInvoked != null) {
                            if (!$util.isObject(object.actionInvoked))
                                throw TypeError(".prodigy.api.v1.ApiMessage.actionInvoked: object expected");
                            message.actionInvoked = $root.prodigy.api.v1.ActionInvokedEvent.fromObject(object.actionInvoked, long + 1);
                        }
                        if (object.postNotificationRequest != null) {
                            if (!$util.isObject(object.postNotificationRequest))
                                throw TypeError(".prodigy.api.v1.ApiMessage.postNotificationRequest: object expected");
                            message.postNotificationRequest = $root.prodigy.api.v1.PostNotificationRequest.fromObject(object.postNotificationRequest, long + 1);
                        }
                        if (object.postNotificationResponse != null) {
                            if (!$util.isObject(object.postNotificationResponse))
                                throw TypeError(".prodigy.api.v1.ApiMessage.postNotificationResponse: object expected");
                            message.postNotificationResponse = $root.prodigy.api.v1.PostNotificationResponse.fromObject(object.postNotificationResponse, long + 1);
                        }
                        if (object.dismissNotificationRequest != null) {
                            if (!$util.isObject(object.dismissNotificationRequest))
                                throw TypeError(".prodigy.api.v1.ApiMessage.dismissNotificationRequest: object expected");
                            message.dismissNotificationRequest = $root.prodigy.api.v1.DismissNotificationRequest.fromObject(object.dismissNotificationRequest, long + 1);
                        }
                        if (object.dialRequest != null) {
                            if (!$util.isObject(object.dialRequest))
                                throw TypeError(".prodigy.api.v1.ApiMessage.dialRequest: object expected");
                            message.dialRequest = $root.prodigy.api.v1.DialRequest.fromObject(object.dialRequest, long + 1);
                        }
                        if (object.answerCallRequest != null) {
                            if (!$util.isObject(object.answerCallRequest))
                                throw TypeError(".prodigy.api.v1.ApiMessage.answerCallRequest: object expected");
                            message.answerCallRequest = $root.prodigy.api.v1.AnswerCallRequest.fromObject(object.answerCallRequest, long + 1);
                        }
                        if (object.hangupRequest != null) {
                            if (!$util.isObject(object.hangupRequest))
                                throw TypeError(".prodigy.api.v1.ApiMessage.hangupRequest: object expected");
                            message.hangupRequest = $root.prodigy.api.v1.HangupRequest.fromObject(object.hangupRequest, long + 1);
                        }
                        if (object.sendDtmfRequest != null) {
                            if (!$util.isObject(object.sendDtmfRequest))
                                throw TypeError(".prodigy.api.v1.ApiMessage.sendDtmfRequest: object expected");
                            message.sendDtmfRequest = $root.prodigy.api.v1.SendDtmfRequest.fromObject(object.sendDtmfRequest, long + 1);
                        }
                        if (object.phoneCommandResponse != null) {
                            if (!$util.isObject(object.phoneCommandResponse))
                                throw TypeError(".prodigy.api.v1.ApiMessage.phoneCommandResponse: object expected");
                            message.phoneCommandResponse = $root.prodigy.api.v1.PhoneCommandResponse.fromObject(object.phoneCommandResponse, long + 1);
                        }
                        if (object.gpsReport != null) {
                            if (!$util.isObject(object.gpsReport))
                                throw TypeError(".prodigy.api.v1.ApiMessage.gpsReport: object expected");
                            message.gpsReport = $root.prodigy.api.v1.GpsReport.fromObject(object.gpsReport, long + 1);
                        }
                        if (object.batteryReport != null) {
                            if (!$util.isObject(object.batteryReport))
                                throw TypeError(".prodigy.api.v1.ApiMessage.batteryReport: object expected");
                            message.batteryReport = $root.prodigy.api.v1.BatteryReport.fromObject(object.batteryReport, long + 1);
                        }
                        if (object.connectivityReport != null) {
                            if (!$util.isObject(object.connectivityReport))
                                throw TypeError(".prodigy.api.v1.ApiMessage.connectivityReport: object expected");
                            message.connectivityReport = $root.prodigy.api.v1.ConnectivityReport.fromObject(object.connectivityReport, long + 1);
                        }
                        if (object.timeReport != null) {
                            if (!$util.isObject(object.timeReport))
                                throw TypeError(".prodigy.api.v1.ApiMessage.timeReport: object expected");
                            message.timeReport = $root.prodigy.api.v1.TimeReport.fromObject(object.timeReport, long + 1);
                        }
                        return message;
                    };
    
                    /**
                     * Creates a plain object from an ApiMessage message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.ApiMessage
                     * @static
                     * @param {prodigy.api.v1.ApiMessage} message ApiMessage
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    ApiMessage.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults)
                            if ($util.Long) {
                                var long = new $util.Long(0, 0, true);
                                object.requestId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : typeof BigInt !== "undefined" && options.longs === BigInt ? long.toBigInt() : long;
                            } else
                                object.requestId = options.longs === String ? "0" : typeof BigInt !== "undefined" && options.longs === BigInt ? BigInt("0") : 0;
                        if (message.requestId != null && Object.hasOwnProperty.call(message, "requestId"))
                            if (typeof BigInt !== "undefined" && options.longs === BigInt)
                                object.requestId = typeof message.requestId === "number" ? BigInt(message.requestId) : $util.Long.fromBits(message.requestId.low >>> 0, message.requestId.high >>> 0, true).toBigInt();
                            else if (typeof message.requestId === "number")
                                object.requestId = options.longs === String ? String(message.requestId) : message.requestId;
                            else
                                object.requestId = options.longs === String ? $util.Long.prototype.toString.call(message.requestId) : options.longs === Number ? new $util.LongBits(message.requestId.low >>> 0, message.requestId.high >>> 0).toNumber(true) : message.requestId;
                        if (message.error != null && Object.hasOwnProperty.call(message, "error")) {
                            object.error = $root.prodigy.api.v1.Error.toObject(message.error, options, q + 1);
                            if (options.oneofs)
                                object.payload = "error";
                        }
                        if (message.ack != null && Object.hasOwnProperty.call(message, "ack")) {
                            object.ack = $root.prodigy.api.v1.Ack.toObject(message.ack, options, q + 1);
                            if (options.oneofs)
                                object.payload = "ack";
                        }
                        if (message.ping != null && Object.hasOwnProperty.call(message, "ping")) {
                            object.ping = $root.prodigy.api.v1.Ping.toObject(message.ping, options, q + 1);
                            if (options.oneofs)
                                object.payload = "ping";
                        }
                        if (message.pong != null && Object.hasOwnProperty.call(message, "pong")) {
                            object.pong = $root.prodigy.api.v1.Pong.toObject(message.pong, options, q + 1);
                            if (options.oneofs)
                                object.payload = "pong";
                        }
                        if (message.clientHello != null && Object.hasOwnProperty.call(message, "clientHello")) {
                            object.clientHello = $root.prodigy.api.v1.ClientHello.toObject(message.clientHello, options, q + 1);
                            if (options.oneofs)
                                object.payload = "clientHello";
                        }
                        if (message.serverHello != null && Object.hasOwnProperty.call(message, "serverHello")) {
                            object.serverHello = $root.prodigy.api.v1.ServerHello.toObject(message.serverHello, options, q + 1);
                            if (options.oneofs)
                                object.payload = "serverHello";
                        }
                        if (message.authRequired != null && Object.hasOwnProperty.call(message, "authRequired")) {
                            object.authRequired = $root.prodigy.api.v1.AuthRequired.toObject(message.authRequired, options, q + 1);
                            if (options.oneofs)
                                object.payload = "authRequired";
                        }
                        if (message.authResponse != null && Object.hasOwnProperty.call(message, "authResponse")) {
                            object.authResponse = $root.prodigy.api.v1.AuthResponse.toObject(message.authResponse, options, q + 1);
                            if (options.oneofs)
                                object.payload = "authResponse";
                        }
                        if (message.authReject != null && Object.hasOwnProperty.call(message, "authReject")) {
                            object.authReject = $root.prodigy.api.v1.AuthReject.toObject(message.authReject, options, q + 1);
                            if (options.oneofs)
                                object.payload = "authReject";
                        }
                        if (message.pairingChallenge != null && Object.hasOwnProperty.call(message, "pairingChallenge")) {
                            object.pairingChallenge = $root.prodigy.api.v1.PairingChallenge.toObject(message.pairingChallenge, options, q + 1);
                            if (options.oneofs)
                                object.payload = "pairingChallenge";
                        }
                        if (message.pairingResponse != null && Object.hasOwnProperty.call(message, "pairingResponse")) {
                            object.pairingResponse = $root.prodigy.api.v1.PairingResponse.toObject(message.pairingResponse, options, q + 1);
                            if (options.oneofs)
                                object.payload = "pairingResponse";
                        }
                        if (message.subscribeRequest != null && Object.hasOwnProperty.call(message, "subscribeRequest")) {
                            object.subscribeRequest = $root.prodigy.api.v1.SubscribeRequest.toObject(message.subscribeRequest, options, q + 1);
                            if (options.oneofs)
                                object.payload = "subscribeRequest";
                        }
                        if (message.subscribeResponse != null && Object.hasOwnProperty.call(message, "subscribeResponse")) {
                            object.subscribeResponse = $root.prodigy.api.v1.SubscribeResponse.toObject(message.subscribeResponse, options, q + 1);
                            if (options.oneofs)
                                object.payload = "subscribeResponse";
                        }
                        if (message.unsubscribeRequest != null && Object.hasOwnProperty.call(message, "unsubscribeRequest")) {
                            object.unsubscribeRequest = $root.prodigy.api.v1.UnsubscribeRequest.toObject(message.unsubscribeRequest, options, q + 1);
                            if (options.oneofs)
                                object.payload = "unsubscribeRequest";
                        }
                        if (message.getCapabilitiesRequest != null && Object.hasOwnProperty.call(message, "getCapabilitiesRequest")) {
                            object.getCapabilitiesRequest = $root.prodigy.api.v1.GetCapabilitiesRequest.toObject(message.getCapabilitiesRequest, options, q + 1);
                            if (options.oneofs)
                                object.payload = "getCapabilitiesRequest";
                        }
                        if (message.capabilitiesResponse != null && Object.hasOwnProperty.call(message, "capabilitiesResponse")) {
                            object.capabilitiesResponse = $root.prodigy.api.v1.CapabilitiesResponse.toObject(message.capabilitiesResponse, options, q + 1);
                            if (options.oneofs)
                                object.payload = "capabilitiesResponse";
                        }
                        if (message.mediaStatus != null && Object.hasOwnProperty.call(message, "mediaStatus")) {
                            object.mediaStatus = $root.prodigy.api.v1.MediaStatus.toObject(message.mediaStatus, options, q + 1);
                            if (options.oneofs)
                                object.payload = "mediaStatus";
                        }
                        if (message.navigationStatus != null && Object.hasOwnProperty.call(message, "navigationStatus")) {
                            object.navigationStatus = $root.prodigy.api.v1.NavigationStatus.toObject(message.navigationStatus, options, q + 1);
                            if (options.oneofs)
                                object.payload = "navigationStatus";
                        }
                        if (message.projectionStatus != null && Object.hasOwnProperty.call(message, "projectionStatus")) {
                            object.projectionStatus = $root.prodigy.api.v1.ProjectionStatus.toObject(message.projectionStatus, options, q + 1);
                            if (options.oneofs)
                                object.payload = "projectionStatus";
                        }
                        if (message.phoneStatus != null && Object.hasOwnProperty.call(message, "phoneStatus")) {
                            object.phoneStatus = $root.prodigy.api.v1.PhoneStatus.toObject(message.phoneStatus, options, q + 1);
                            if (options.oneofs)
                                object.payload = "phoneStatus";
                        }
                        if (message.systemStatus != null && Object.hasOwnProperty.call(message, "systemStatus")) {
                            object.systemStatus = $root.prodigy.api.v1.SystemStatus.toObject(message.systemStatus, options, q + 1);
                            if (options.oneofs)
                                object.payload = "systemStatus";
                        }
                        if (message.listActionsRequest != null && Object.hasOwnProperty.call(message, "listActionsRequest")) {
                            object.listActionsRequest = $root.prodigy.api.v1.ListActionsRequest.toObject(message.listActionsRequest, options, q + 1);
                            if (options.oneofs)
                                object.payload = "listActionsRequest";
                        }
                        if (message.listActionsResponse != null && Object.hasOwnProperty.call(message, "listActionsResponse")) {
                            object.listActionsResponse = $root.prodigy.api.v1.ListActionsResponse.toObject(message.listActionsResponse, options, q + 1);
                            if (options.oneofs)
                                object.payload = "listActionsResponse";
                        }
                        if (message.dispatchActionRequest != null && Object.hasOwnProperty.call(message, "dispatchActionRequest")) {
                            object.dispatchActionRequest = $root.prodigy.api.v1.DispatchActionRequest.toObject(message.dispatchActionRequest, options, q + 1);
                            if (options.oneofs)
                                object.payload = "dispatchActionRequest";
                        }
                        if (message.dispatchActionResponse != null && Object.hasOwnProperty.call(message, "dispatchActionResponse")) {
                            object.dispatchActionResponse = $root.prodigy.api.v1.DispatchActionResponse.toObject(message.dispatchActionResponse, options, q + 1);
                            if (options.oneofs)
                                object.payload = "dispatchActionResponse";
                        }
                        if (message.registerActionsRequest != null && Object.hasOwnProperty.call(message, "registerActionsRequest")) {
                            object.registerActionsRequest = $root.prodigy.api.v1.RegisterActionsRequest.toObject(message.registerActionsRequest, options, q + 1);
                            if (options.oneofs)
                                object.payload = "registerActionsRequest";
                        }
                        if (message.registerActionsResponse != null && Object.hasOwnProperty.call(message, "registerActionsResponse")) {
                            object.registerActionsResponse = $root.prodigy.api.v1.RegisterActionsResponse.toObject(message.registerActionsResponse, options, q + 1);
                            if (options.oneofs)
                                object.payload = "registerActionsResponse";
                        }
                        if (message.unregisterActionsRequest != null && Object.hasOwnProperty.call(message, "unregisterActionsRequest")) {
                            object.unregisterActionsRequest = $root.prodigy.api.v1.UnregisterActionsRequest.toObject(message.unregisterActionsRequest, options, q + 1);
                            if (options.oneofs)
                                object.payload = "unregisterActionsRequest";
                        }
                        if (message.actionInvoked != null && Object.hasOwnProperty.call(message, "actionInvoked")) {
                            object.actionInvoked = $root.prodigy.api.v1.ActionInvokedEvent.toObject(message.actionInvoked, options, q + 1);
                            if (options.oneofs)
                                object.payload = "actionInvoked";
                        }
                        if (message.postNotificationRequest != null && Object.hasOwnProperty.call(message, "postNotificationRequest")) {
                            object.postNotificationRequest = $root.prodigy.api.v1.PostNotificationRequest.toObject(message.postNotificationRequest, options, q + 1);
                            if (options.oneofs)
                                object.payload = "postNotificationRequest";
                        }
                        if (message.postNotificationResponse != null && Object.hasOwnProperty.call(message, "postNotificationResponse")) {
                            object.postNotificationResponse = $root.prodigy.api.v1.PostNotificationResponse.toObject(message.postNotificationResponse, options, q + 1);
                            if (options.oneofs)
                                object.payload = "postNotificationResponse";
                        }
                        if (message.dismissNotificationRequest != null && Object.hasOwnProperty.call(message, "dismissNotificationRequest")) {
                            object.dismissNotificationRequest = $root.prodigy.api.v1.DismissNotificationRequest.toObject(message.dismissNotificationRequest, options, q + 1);
                            if (options.oneofs)
                                object.payload = "dismissNotificationRequest";
                        }
                        if (message.dialRequest != null && Object.hasOwnProperty.call(message, "dialRequest")) {
                            object.dialRequest = $root.prodigy.api.v1.DialRequest.toObject(message.dialRequest, options, q + 1);
                            if (options.oneofs)
                                object.payload = "dialRequest";
                        }
                        if (message.answerCallRequest != null && Object.hasOwnProperty.call(message, "answerCallRequest")) {
                            object.answerCallRequest = $root.prodigy.api.v1.AnswerCallRequest.toObject(message.answerCallRequest, options, q + 1);
                            if (options.oneofs)
                                object.payload = "answerCallRequest";
                        }
                        if (message.hangupRequest != null && Object.hasOwnProperty.call(message, "hangupRequest")) {
                            object.hangupRequest = $root.prodigy.api.v1.HangupRequest.toObject(message.hangupRequest, options, q + 1);
                            if (options.oneofs)
                                object.payload = "hangupRequest";
                        }
                        if (message.sendDtmfRequest != null && Object.hasOwnProperty.call(message, "sendDtmfRequest")) {
                            object.sendDtmfRequest = $root.prodigy.api.v1.SendDtmfRequest.toObject(message.sendDtmfRequest, options, q + 1);
                            if (options.oneofs)
                                object.payload = "sendDtmfRequest";
                        }
                        if (message.phoneCommandResponse != null && Object.hasOwnProperty.call(message, "phoneCommandResponse")) {
                            object.phoneCommandResponse = $root.prodigy.api.v1.PhoneCommandResponse.toObject(message.phoneCommandResponse, options, q + 1);
                            if (options.oneofs)
                                object.payload = "phoneCommandResponse";
                        }
                        if (message.gpsReport != null && Object.hasOwnProperty.call(message, "gpsReport")) {
                            object.gpsReport = $root.prodigy.api.v1.GpsReport.toObject(message.gpsReport, options, q + 1);
                            if (options.oneofs)
                                object.payload = "gpsReport";
                        }
                        if (message.batteryReport != null && Object.hasOwnProperty.call(message, "batteryReport")) {
                            object.batteryReport = $root.prodigy.api.v1.BatteryReport.toObject(message.batteryReport, options, q + 1);
                            if (options.oneofs)
                                object.payload = "batteryReport";
                        }
                        if (message.connectivityReport != null && Object.hasOwnProperty.call(message, "connectivityReport")) {
                            object.connectivityReport = $root.prodigy.api.v1.ConnectivityReport.toObject(message.connectivityReport, options, q + 1);
                            if (options.oneofs)
                                object.payload = "connectivityReport";
                        }
                        if (message.timeReport != null && Object.hasOwnProperty.call(message, "timeReport")) {
                            object.timeReport = $root.prodigy.api.v1.TimeReport.toObject(message.timeReport, options, q + 1);
                            if (options.oneofs)
                                object.payload = "timeReport";
                        }
                        return object;
                    };
    
                    /**
                     * Converts this ApiMessage to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.ApiMessage
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    ApiMessage.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for ApiMessage
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.ApiMessage
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    ApiMessage.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.ApiMessage";
                    };
    
                    return ApiMessage;
                })();
    
                /**
                 * Topic enum.
                 * @name prodigy.api.v1.Topic
                 * @enum {number}
                 * @property {number} TOPIC_UNSPECIFIED=0 TOPIC_UNSPECIFIED value
                 * @property {number} TOPIC_MEDIA=1 TOPIC_MEDIA value
                 * @property {number} TOPIC_NAVIGATION=2 TOPIC_NAVIGATION value
                 * @property {number} TOPIC_PROJECTION=3 TOPIC_PROJECTION value
                 * @property {number} TOPIC_PHONE=4 TOPIC_PHONE value
                 * @property {number} TOPIC_SYSTEM=5 TOPIC_SYSTEM value
                 */
                v1.Topic = (function() {
                    var valuesById = {}, values = Object.create(valuesById);
                    values[valuesById[0] = "TOPIC_UNSPECIFIED"] = 0;
                    values[valuesById[1] = "TOPIC_MEDIA"] = 1;
                    values[valuesById[2] = "TOPIC_NAVIGATION"] = 2;
                    values[valuesById[3] = "TOPIC_PROJECTION"] = 3;
                    values[valuesById[4] = "TOPIC_PHONE"] = 4;
                    values[valuesById[5] = "TOPIC_SYSTEM"] = 5;
                    return values;
                })();
    
                /**
                 * ErrorCode enum.
                 * @name prodigy.api.v1.ErrorCode
                 * @enum {number}
                 * @property {number} ERROR_CODE_UNSPECIFIED=0 ERROR_CODE_UNSPECIFIED value
                 * @property {number} ERROR_CODE_INVALID_REQUEST=1 ERROR_CODE_INVALID_REQUEST value
                 * @property {number} ERROR_CODE_UNSUPPORTED_VERSION=2 ERROR_CODE_UNSUPPORTED_VERSION value
                 * @property {number} ERROR_CODE_NOT_AUTHENTICATED=3 ERROR_CODE_NOT_AUTHENTICATED value
                 * @property {number} ERROR_CODE_AUTH_FAILED=4 ERROR_CODE_AUTH_FAILED value
                 * @property {number} ERROR_CODE_PAIRING_WINDOW_CLOSED=5 ERROR_CODE_PAIRING_WINDOW_CLOSED value
                 * @property {number} ERROR_CODE_NOT_FOUND=6 ERROR_CODE_NOT_FOUND value
                 * @property {number} ERROR_CODE_UNKNOWN_ACTION=7 ERROR_CODE_UNKNOWN_ACTION value
                 * @property {number} ERROR_CODE_REJECTED=8 ERROR_CODE_REJECTED value
                 * @property {number} ERROR_CODE_UNAVAILABLE=9 ERROR_CODE_UNAVAILABLE value
                 * @property {number} ERROR_CODE_INTERNAL=10 ERROR_CODE_INTERNAL value
                 */
                v1.ErrorCode = (function() {
                    var valuesById = {}, values = Object.create(valuesById);
                    values[valuesById[0] = "ERROR_CODE_UNSPECIFIED"] = 0;
                    values[valuesById[1] = "ERROR_CODE_INVALID_REQUEST"] = 1;
                    values[valuesById[2] = "ERROR_CODE_UNSUPPORTED_VERSION"] = 2;
                    values[valuesById[3] = "ERROR_CODE_NOT_AUTHENTICATED"] = 3;
                    values[valuesById[4] = "ERROR_CODE_AUTH_FAILED"] = 4;
                    values[valuesById[5] = "ERROR_CODE_PAIRING_WINDOW_CLOSED"] = 5;
                    values[valuesById[6] = "ERROR_CODE_NOT_FOUND"] = 6;
                    values[valuesById[7] = "ERROR_CODE_UNKNOWN_ACTION"] = 7;
                    values[valuesById[8] = "ERROR_CODE_REJECTED"] = 8;
                    values[valuesById[9] = "ERROR_CODE_UNAVAILABLE"] = 9;
                    values[valuesById[10] = "ERROR_CODE_INTERNAL"] = 10;
                    return values;
                })();
    
                v1.Error = (function() {
    
                    /**
                     * Properties of an Error.
                     * @memberof prodigy.api.v1
                     * @interface IError
                     * @property {prodigy.api.v1.ErrorCode|null} [code] Error code
                     * @property {string|null} [message] Error message
                     */
    
                    /**
                     * Constructs a new Error.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents an Error.
                     * @implements IError
                     * @constructor
                     * @param {prodigy.api.v1.IError=} [properties] Properties to set
                     */
                    function Error(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * Error code.
                     * @member {prodigy.api.v1.ErrorCode} code
                     * @memberof prodigy.api.v1.Error
                     * @instance
                     */
                    Error.prototype.code = 0;
    
                    /**
                     * Error message.
                     * @member {string} message
                     * @memberof prodigy.api.v1.Error
                     * @instance
                     */
                    Error.prototype.message = "";
    
                    /**
                     * Creates a new Error instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.Error
                     * @static
                     * @param {prodigy.api.v1.IError=} [properties] Properties to set
                     * @returns {prodigy.api.v1.Error} Error instance
                     */
                    Error.create = function create(properties) {
                        return new Error(properties);
                    };
    
                    /**
                     * Encodes the specified Error message. Does not implicitly {@link prodigy.api.v1.Error.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.Error
                     * @static
                     * @param {prodigy.api.v1.IError} message Error message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    Error.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.code != null && Object.hasOwnProperty.call(message, "code"))
                            writer.uint32(/* id 1, wireType 0 =*/8).int32(message.code);
                        if (message.message != null && Object.hasOwnProperty.call(message, "message"))
                            writer.uint32(/* id 2, wireType 2 =*/18).string(message.message);
                        return writer;
                    };
    
                    /**
                     * Decodes an Error message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.Error
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.Error} Error
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    Error.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.Error();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.code = reader.int32();
                                    break;
                                }
                            case 2: {
                                    message.message = reader.string();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies an Error message.
                     * @function verify
                     * @memberof prodigy.api.v1.Error
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    Error.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.code != null && Object.hasOwnProperty.call(message, "code"))
                            switch (message.code) {
                            default:
                                return "code: enum value expected";
                            case 0:
                            case 1:
                            case 2:
                            case 3:
                            case 4:
                            case 5:
                            case 6:
                            case 7:
                            case 8:
                            case 9:
                            case 10:
                                break;
                            }
                        if (message.message != null && Object.hasOwnProperty.call(message, "message"))
                            if (!$util.isString(message.message))
                                return "message: string expected";
                        return null;
                    };
    
                    /**
                     * Creates an Error message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.Error
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.Error} Error
                     */
                    Error.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.Error)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.Error: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.Error();
                        switch (object.code) {
                        default:
                            if (typeof object.code === "number") {
                                message.code = object.code;
                                break;
                            }
                            break;
                        case "ERROR_CODE_UNSPECIFIED":
                        case 0:
                            message.code = 0;
                            break;
                        case "ERROR_CODE_INVALID_REQUEST":
                        case 1:
                            message.code = 1;
                            break;
                        case "ERROR_CODE_UNSUPPORTED_VERSION":
                        case 2:
                            message.code = 2;
                            break;
                        case "ERROR_CODE_NOT_AUTHENTICATED":
                        case 3:
                            message.code = 3;
                            break;
                        case "ERROR_CODE_AUTH_FAILED":
                        case 4:
                            message.code = 4;
                            break;
                        case "ERROR_CODE_PAIRING_WINDOW_CLOSED":
                        case 5:
                            message.code = 5;
                            break;
                        case "ERROR_CODE_NOT_FOUND":
                        case 6:
                            message.code = 6;
                            break;
                        case "ERROR_CODE_UNKNOWN_ACTION":
                        case 7:
                            message.code = 7;
                            break;
                        case "ERROR_CODE_REJECTED":
                        case 8:
                            message.code = 8;
                            break;
                        case "ERROR_CODE_UNAVAILABLE":
                        case 9:
                            message.code = 9;
                            break;
                        case "ERROR_CODE_INTERNAL":
                        case 10:
                            message.code = 10;
                            break;
                        }
                        if (object.message != null)
                            message.message = String(object.message);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from an Error message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.Error
                     * @static
                     * @param {prodigy.api.v1.Error} message Error
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    Error.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.code = options.enums === String ? "ERROR_CODE_UNSPECIFIED" : 0;
                            object.message = "";
                        }
                        if (message.code != null && Object.hasOwnProperty.call(message, "code"))
                            object.code = options.enums === String ? $root.prodigy.api.v1.ErrorCode[message.code] === undefined ? message.code : $root.prodigy.api.v1.ErrorCode[message.code] : message.code;
                        if (message.message != null && Object.hasOwnProperty.call(message, "message"))
                            object.message = message.message;
                        return object;
                    };
    
                    /**
                     * Converts this Error to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.Error
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    Error.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for Error
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.Error
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    Error.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.Error";
                    };
    
                    return Error;
                })();
    
                v1.Ack = (function() {
    
                    /**
                     * Properties of an Ack.
                     * @memberof prodigy.api.v1
                     * @interface IAck
                     */
    
                    /**
                     * Constructs a new Ack.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents an Ack.
                     * @implements IAck
                     * @constructor
                     * @param {prodigy.api.v1.IAck=} [properties] Properties to set
                     */
                    function Ack(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * Creates a new Ack instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.Ack
                     * @static
                     * @param {prodigy.api.v1.IAck=} [properties] Properties to set
                     * @returns {prodigy.api.v1.Ack} Ack instance
                     */
                    Ack.create = function create(properties) {
                        return new Ack(properties);
                    };
    
                    /**
                     * Encodes the specified Ack message. Does not implicitly {@link prodigy.api.v1.Ack.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.Ack
                     * @static
                     * @param {prodigy.api.v1.IAck} message Ack message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    Ack.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        return writer;
                    };
    
                    /**
                     * Decodes an Ack message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.Ack
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.Ack} Ack
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    Ack.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.Ack();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies an Ack message.
                     * @function verify
                     * @memberof prodigy.api.v1.Ack
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    Ack.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        return null;
                    };
    
                    /**
                     * Creates an Ack message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.Ack
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.Ack} Ack
                     */
                    Ack.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.Ack)
                            return object;
                        return new $root.prodigy.api.v1.Ack();
                    };
    
                    /**
                     * Creates a plain object from an Ack message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.Ack
                     * @static
                     * @param {prodigy.api.v1.Ack} message Ack
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    Ack.toObject = function toObject() {
                        return {};
                    };
    
                    /**
                     * Converts this Ack to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.Ack
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    Ack.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for Ack
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.Ack
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    Ack.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.Ack";
                    };
    
                    return Ack;
                })();
    
                v1.Ping = (function() {
    
                    /**
                     * Properties of a Ping.
                     * @memberof prodigy.api.v1
                     * @interface IPing
                     */
    
                    /**
                     * Constructs a new Ping.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a Ping.
                     * @implements IPing
                     * @constructor
                     * @param {prodigy.api.v1.IPing=} [properties] Properties to set
                     */
                    function Ping(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * Creates a new Ping instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.Ping
                     * @static
                     * @param {prodigy.api.v1.IPing=} [properties] Properties to set
                     * @returns {prodigy.api.v1.Ping} Ping instance
                     */
                    Ping.create = function create(properties) {
                        return new Ping(properties);
                    };
    
                    /**
                     * Encodes the specified Ping message. Does not implicitly {@link prodigy.api.v1.Ping.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.Ping
                     * @static
                     * @param {prodigy.api.v1.IPing} message Ping message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    Ping.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        return writer;
                    };
    
                    /**
                     * Decodes a Ping message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.Ping
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.Ping} Ping
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    Ping.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.Ping();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a Ping message.
                     * @function verify
                     * @memberof prodigy.api.v1.Ping
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    Ping.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        return null;
                    };
    
                    /**
                     * Creates a Ping message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.Ping
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.Ping} Ping
                     */
                    Ping.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.Ping)
                            return object;
                        return new $root.prodigy.api.v1.Ping();
                    };
    
                    /**
                     * Creates a plain object from a Ping message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.Ping
                     * @static
                     * @param {prodigy.api.v1.Ping} message Ping
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    Ping.toObject = function toObject() {
                        return {};
                    };
    
                    /**
                     * Converts this Ping to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.Ping
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    Ping.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for Ping
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.Ping
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    Ping.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.Ping";
                    };
    
                    return Ping;
                })();
    
                v1.Pong = (function() {
    
                    /**
                     * Properties of a Pong.
                     * @memberof prodigy.api.v1
                     * @interface IPong
                     */
    
                    /**
                     * Constructs a new Pong.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a Pong.
                     * @implements IPong
                     * @constructor
                     * @param {prodigy.api.v1.IPong=} [properties] Properties to set
                     */
                    function Pong(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * Creates a new Pong instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.Pong
                     * @static
                     * @param {prodigy.api.v1.IPong=} [properties] Properties to set
                     * @returns {prodigy.api.v1.Pong} Pong instance
                     */
                    Pong.create = function create(properties) {
                        return new Pong(properties);
                    };
    
                    /**
                     * Encodes the specified Pong message. Does not implicitly {@link prodigy.api.v1.Pong.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.Pong
                     * @static
                     * @param {prodigy.api.v1.IPong} message Pong message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    Pong.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        return writer;
                    };
    
                    /**
                     * Decodes a Pong message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.Pong
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.Pong} Pong
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    Pong.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.Pong();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a Pong message.
                     * @function verify
                     * @memberof prodigy.api.v1.Pong
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    Pong.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        return null;
                    };
    
                    /**
                     * Creates a Pong message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.Pong
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.Pong} Pong
                     */
                    Pong.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.Pong)
                            return object;
                        return new $root.prodigy.api.v1.Pong();
                    };
    
                    /**
                     * Creates a plain object from a Pong message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.Pong
                     * @static
                     * @param {prodigy.api.v1.Pong} message Pong
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    Pong.toObject = function toObject() {
                        return {};
                    };
    
                    /**
                     * Converts this Pong to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.Pong
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    Pong.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for Pong
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.Pong
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    Pong.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.Pong";
                    };
    
                    return Pong;
                })();
    
                /**
                 * MediaSource enum.
                 * @name prodigy.api.v1.MediaSource
                 * @enum {number}
                 * @property {number} MEDIA_SOURCE_UNSPECIFIED=0 MEDIA_SOURCE_UNSPECIFIED value
                 * @property {number} MEDIA_SOURCE_NONE=1 MEDIA_SOURCE_NONE value
                 * @property {number} MEDIA_SOURCE_BLUETOOTH=2 MEDIA_SOURCE_BLUETOOTH value
                 * @property {number} MEDIA_SOURCE_ANDROID_AUTO=3 MEDIA_SOURCE_ANDROID_AUTO value
                 */
                v1.MediaSource = (function() {
                    var valuesById = {}, values = Object.create(valuesById);
                    values[valuesById[0] = "MEDIA_SOURCE_UNSPECIFIED"] = 0;
                    values[valuesById[1] = "MEDIA_SOURCE_NONE"] = 1;
                    values[valuesById[2] = "MEDIA_SOURCE_BLUETOOTH"] = 2;
                    values[valuesById[3] = "MEDIA_SOURCE_ANDROID_AUTO"] = 3;
                    return values;
                })();
    
                /**
                 * PlaybackState enum.
                 * @name prodigy.api.v1.PlaybackState
                 * @enum {number}
                 * @property {number} PLAYBACK_STATE_UNSPECIFIED=0 PLAYBACK_STATE_UNSPECIFIED value
                 * @property {number} PLAYBACK_STATE_STOPPED=1 PLAYBACK_STATE_STOPPED value
                 * @property {number} PLAYBACK_STATE_PLAYING=2 PLAYBACK_STATE_PLAYING value
                 * @property {number} PLAYBACK_STATE_PAUSED=3 PLAYBACK_STATE_PAUSED value
                 */
                v1.PlaybackState = (function() {
                    var valuesById = {}, values = Object.create(valuesById);
                    values[valuesById[0] = "PLAYBACK_STATE_UNSPECIFIED"] = 0;
                    values[valuesById[1] = "PLAYBACK_STATE_STOPPED"] = 1;
                    values[valuesById[2] = "PLAYBACK_STATE_PLAYING"] = 2;
                    values[valuesById[3] = "PLAYBACK_STATE_PAUSED"] = 3;
                    return values;
                })();
    
                v1.MediaStatus = (function() {
    
                    /**
                     * Properties of a MediaStatus.
                     * @memberof prodigy.api.v1
                     * @interface IMediaStatus
                     * @property {boolean|null} [hasMedia] MediaStatus hasMedia
                     * @property {string|null} [title] MediaStatus title
                     * @property {string|null} [artist] MediaStatus artist
                     * @property {string|null} [album] MediaStatus album
                     * @property {prodigy.api.v1.PlaybackState|null} [playbackState] MediaStatus playbackState
                     * @property {prodigy.api.v1.MediaSource|null} [source] MediaStatus source
                     * @property {string|null} [appName] MediaStatus appName
                     */
    
                    /**
                     * Constructs a new MediaStatus.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a MediaStatus.
                     * @implements IMediaStatus
                     * @constructor
                     * @param {prodigy.api.v1.IMediaStatus=} [properties] Properties to set
                     */
                    function MediaStatus(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * MediaStatus hasMedia.
                     * @member {boolean} hasMedia
                     * @memberof prodigy.api.v1.MediaStatus
                     * @instance
                     */
                    MediaStatus.prototype.hasMedia = false;
    
                    /**
                     * MediaStatus title.
                     * @member {string} title
                     * @memberof prodigy.api.v1.MediaStatus
                     * @instance
                     */
                    MediaStatus.prototype.title = "";
    
                    /**
                     * MediaStatus artist.
                     * @member {string} artist
                     * @memberof prodigy.api.v1.MediaStatus
                     * @instance
                     */
                    MediaStatus.prototype.artist = "";
    
                    /**
                     * MediaStatus album.
                     * @member {string} album
                     * @memberof prodigy.api.v1.MediaStatus
                     * @instance
                     */
                    MediaStatus.prototype.album = "";
    
                    /**
                     * MediaStatus playbackState.
                     * @member {prodigy.api.v1.PlaybackState} playbackState
                     * @memberof prodigy.api.v1.MediaStatus
                     * @instance
                     */
                    MediaStatus.prototype.playbackState = 0;
    
                    /**
                     * MediaStatus source.
                     * @member {prodigy.api.v1.MediaSource} source
                     * @memberof prodigy.api.v1.MediaStatus
                     * @instance
                     */
                    MediaStatus.prototype.source = 0;
    
                    /**
                     * MediaStatus appName.
                     * @member {string} appName
                     * @memberof prodigy.api.v1.MediaStatus
                     * @instance
                     */
                    MediaStatus.prototype.appName = "";
    
                    /**
                     * Creates a new MediaStatus instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.MediaStatus
                     * @static
                     * @param {prodigy.api.v1.IMediaStatus=} [properties] Properties to set
                     * @returns {prodigy.api.v1.MediaStatus} MediaStatus instance
                     */
                    MediaStatus.create = function create(properties) {
                        return new MediaStatus(properties);
                    };
    
                    /**
                     * Encodes the specified MediaStatus message. Does not implicitly {@link prodigy.api.v1.MediaStatus.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.MediaStatus
                     * @static
                     * @param {prodigy.api.v1.IMediaStatus} message MediaStatus message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    MediaStatus.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.hasMedia != null && Object.hasOwnProperty.call(message, "hasMedia"))
                            writer.uint32(/* id 1, wireType 0 =*/8).bool(message.hasMedia);
                        if (message.title != null && Object.hasOwnProperty.call(message, "title"))
                            writer.uint32(/* id 2, wireType 2 =*/18).string(message.title);
                        if (message.artist != null && Object.hasOwnProperty.call(message, "artist"))
                            writer.uint32(/* id 3, wireType 2 =*/26).string(message.artist);
                        if (message.album != null && Object.hasOwnProperty.call(message, "album"))
                            writer.uint32(/* id 4, wireType 2 =*/34).string(message.album);
                        if (message.playbackState != null && Object.hasOwnProperty.call(message, "playbackState"))
                            writer.uint32(/* id 5, wireType 0 =*/40).int32(message.playbackState);
                        if (message.source != null && Object.hasOwnProperty.call(message, "source"))
                            writer.uint32(/* id 6, wireType 0 =*/48).int32(message.source);
                        if (message.appName != null && Object.hasOwnProperty.call(message, "appName"))
                            writer.uint32(/* id 7, wireType 2 =*/58).string(message.appName);
                        return writer;
                    };
    
                    /**
                     * Decodes a MediaStatus message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.MediaStatus
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.MediaStatus} MediaStatus
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    MediaStatus.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.MediaStatus();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.hasMedia = reader.bool();
                                    break;
                                }
                            case 2: {
                                    message.title = reader.string();
                                    break;
                                }
                            case 3: {
                                    message.artist = reader.string();
                                    break;
                                }
                            case 4: {
                                    message.album = reader.string();
                                    break;
                                }
                            case 5: {
                                    message.playbackState = reader.int32();
                                    break;
                                }
                            case 6: {
                                    message.source = reader.int32();
                                    break;
                                }
                            case 7: {
                                    message.appName = reader.string();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a MediaStatus message.
                     * @function verify
                     * @memberof prodigy.api.v1.MediaStatus
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    MediaStatus.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.hasMedia != null && Object.hasOwnProperty.call(message, "hasMedia"))
                            if (typeof message.hasMedia !== "boolean")
                                return "hasMedia: boolean expected";
                        if (message.title != null && Object.hasOwnProperty.call(message, "title"))
                            if (!$util.isString(message.title))
                                return "title: string expected";
                        if (message.artist != null && Object.hasOwnProperty.call(message, "artist"))
                            if (!$util.isString(message.artist))
                                return "artist: string expected";
                        if (message.album != null && Object.hasOwnProperty.call(message, "album"))
                            if (!$util.isString(message.album))
                                return "album: string expected";
                        if (message.playbackState != null && Object.hasOwnProperty.call(message, "playbackState"))
                            switch (message.playbackState) {
                            default:
                                return "playbackState: enum value expected";
                            case 0:
                            case 1:
                            case 2:
                            case 3:
                                break;
                            }
                        if (message.source != null && Object.hasOwnProperty.call(message, "source"))
                            switch (message.source) {
                            default:
                                return "source: enum value expected";
                            case 0:
                            case 1:
                            case 2:
                            case 3:
                                break;
                            }
                        if (message.appName != null && Object.hasOwnProperty.call(message, "appName"))
                            if (!$util.isString(message.appName))
                                return "appName: string expected";
                        return null;
                    };
    
                    /**
                     * Creates a MediaStatus message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.MediaStatus
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.MediaStatus} MediaStatus
                     */
                    MediaStatus.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.MediaStatus)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.MediaStatus: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.MediaStatus();
                        if (object.hasMedia != null)
                            message.hasMedia = Boolean(object.hasMedia);
                        if (object.title != null)
                            message.title = String(object.title);
                        if (object.artist != null)
                            message.artist = String(object.artist);
                        if (object.album != null)
                            message.album = String(object.album);
                        switch (object.playbackState) {
                        default:
                            if (typeof object.playbackState === "number") {
                                message.playbackState = object.playbackState;
                                break;
                            }
                            break;
                        case "PLAYBACK_STATE_UNSPECIFIED":
                        case 0:
                            message.playbackState = 0;
                            break;
                        case "PLAYBACK_STATE_STOPPED":
                        case 1:
                            message.playbackState = 1;
                            break;
                        case "PLAYBACK_STATE_PLAYING":
                        case 2:
                            message.playbackState = 2;
                            break;
                        case "PLAYBACK_STATE_PAUSED":
                        case 3:
                            message.playbackState = 3;
                            break;
                        }
                        switch (object.source) {
                        default:
                            if (typeof object.source === "number") {
                                message.source = object.source;
                                break;
                            }
                            break;
                        case "MEDIA_SOURCE_UNSPECIFIED":
                        case 0:
                            message.source = 0;
                            break;
                        case "MEDIA_SOURCE_NONE":
                        case 1:
                            message.source = 1;
                            break;
                        case "MEDIA_SOURCE_BLUETOOTH":
                        case 2:
                            message.source = 2;
                            break;
                        case "MEDIA_SOURCE_ANDROID_AUTO":
                        case 3:
                            message.source = 3;
                            break;
                        }
                        if (object.appName != null)
                            message.appName = String(object.appName);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a MediaStatus message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.MediaStatus
                     * @static
                     * @param {prodigy.api.v1.MediaStatus} message MediaStatus
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    MediaStatus.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.hasMedia = false;
                            object.title = "";
                            object.artist = "";
                            object.album = "";
                            object.playbackState = options.enums === String ? "PLAYBACK_STATE_UNSPECIFIED" : 0;
                            object.source = options.enums === String ? "MEDIA_SOURCE_UNSPECIFIED" : 0;
                            object.appName = "";
                        }
                        if (message.hasMedia != null && Object.hasOwnProperty.call(message, "hasMedia"))
                            object.hasMedia = message.hasMedia;
                        if (message.title != null && Object.hasOwnProperty.call(message, "title"))
                            object.title = message.title;
                        if (message.artist != null && Object.hasOwnProperty.call(message, "artist"))
                            object.artist = message.artist;
                        if (message.album != null && Object.hasOwnProperty.call(message, "album"))
                            object.album = message.album;
                        if (message.playbackState != null && Object.hasOwnProperty.call(message, "playbackState"))
                            object.playbackState = options.enums === String ? $root.prodigy.api.v1.PlaybackState[message.playbackState] === undefined ? message.playbackState : $root.prodigy.api.v1.PlaybackState[message.playbackState] : message.playbackState;
                        if (message.source != null && Object.hasOwnProperty.call(message, "source"))
                            object.source = options.enums === String ? $root.prodigy.api.v1.MediaSource[message.source] === undefined ? message.source : $root.prodigy.api.v1.MediaSource[message.source] : message.source;
                        if (message.appName != null && Object.hasOwnProperty.call(message, "appName"))
                            object.appName = message.appName;
                        return object;
                    };
    
                    /**
                     * Converts this MediaStatus to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.MediaStatus
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    MediaStatus.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for MediaStatus
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.MediaStatus
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    MediaStatus.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.MediaStatus";
                    };
    
                    return MediaStatus;
                })();
    
                /**
                 * ManeuverType enum.
                 * @name prodigy.api.v1.ManeuverType
                 * @enum {number}
                 * @property {number} MANEUVER_TYPE_UNSPECIFIED=0 MANEUVER_TYPE_UNSPECIFIED value
                 * @property {number} MANEUVER_TYPE_OTHER=1 MANEUVER_TYPE_OTHER value
                 * @property {number} MANEUVER_TYPE_DEPART=2 MANEUVER_TYPE_DEPART value
                 * @property {number} MANEUVER_TYPE_STRAIGHT=3 MANEUVER_TYPE_STRAIGHT value
                 * @property {number} MANEUVER_TYPE_KEEP=4 MANEUVER_TYPE_KEEP value
                 * @property {number} MANEUVER_TYPE_TURN=5 MANEUVER_TYPE_TURN value
                 * @property {number} MANEUVER_TYPE_SHARP_TURN=6 MANEUVER_TYPE_SHARP_TURN value
                 * @property {number} MANEUVER_TYPE_SLIGHT_TURN=7 MANEUVER_TYPE_SLIGHT_TURN value
                 * @property {number} MANEUVER_TYPE_U_TURN=8 MANEUVER_TYPE_U_TURN value
                 * @property {number} MANEUVER_TYPE_ON_RAMP=9 MANEUVER_TYPE_ON_RAMP value
                 * @property {number} MANEUVER_TYPE_OFF_RAMP=10 MANEUVER_TYPE_OFF_RAMP value
                 * @property {number} MANEUVER_TYPE_FORK=11 MANEUVER_TYPE_FORK value
                 * @property {number} MANEUVER_TYPE_MERGE=12 MANEUVER_TYPE_MERGE value
                 * @property {number} MANEUVER_TYPE_ROUNDABOUT_ENTER=13 MANEUVER_TYPE_ROUNDABOUT_ENTER value
                 * @property {number} MANEUVER_TYPE_ROUNDABOUT_EXIT=14 MANEUVER_TYPE_ROUNDABOUT_EXIT value
                 * @property {number} MANEUVER_TYPE_ROUNDABOUT_ENTER_AND_EXIT=15 MANEUVER_TYPE_ROUNDABOUT_ENTER_AND_EXIT value
                 * @property {number} MANEUVER_TYPE_FERRY=16 MANEUVER_TYPE_FERRY value
                 * @property {number} MANEUVER_TYPE_DESTINATION=17 MANEUVER_TYPE_DESTINATION value
                 * @property {number} MANEUVER_TYPE_NAME_CHANGE=18 MANEUVER_TYPE_NAME_CHANGE value
                 */
                v1.ManeuverType = (function() {
                    var valuesById = {}, values = Object.create(valuesById);
                    values[valuesById[0] = "MANEUVER_TYPE_UNSPECIFIED"] = 0;
                    values[valuesById[1] = "MANEUVER_TYPE_OTHER"] = 1;
                    values[valuesById[2] = "MANEUVER_TYPE_DEPART"] = 2;
                    values[valuesById[3] = "MANEUVER_TYPE_STRAIGHT"] = 3;
                    values[valuesById[4] = "MANEUVER_TYPE_KEEP"] = 4;
                    values[valuesById[5] = "MANEUVER_TYPE_TURN"] = 5;
                    values[valuesById[6] = "MANEUVER_TYPE_SHARP_TURN"] = 6;
                    values[valuesById[7] = "MANEUVER_TYPE_SLIGHT_TURN"] = 7;
                    values[valuesById[8] = "MANEUVER_TYPE_U_TURN"] = 8;
                    values[valuesById[9] = "MANEUVER_TYPE_ON_RAMP"] = 9;
                    values[valuesById[10] = "MANEUVER_TYPE_OFF_RAMP"] = 10;
                    values[valuesById[11] = "MANEUVER_TYPE_FORK"] = 11;
                    values[valuesById[12] = "MANEUVER_TYPE_MERGE"] = 12;
                    values[valuesById[13] = "MANEUVER_TYPE_ROUNDABOUT_ENTER"] = 13;
                    values[valuesById[14] = "MANEUVER_TYPE_ROUNDABOUT_EXIT"] = 14;
                    values[valuesById[15] = "MANEUVER_TYPE_ROUNDABOUT_ENTER_AND_EXIT"] = 15;
                    values[valuesById[16] = "MANEUVER_TYPE_FERRY"] = 16;
                    values[valuesById[17] = "MANEUVER_TYPE_DESTINATION"] = 17;
                    values[valuesById[18] = "MANEUVER_TYPE_NAME_CHANGE"] = 18;
                    return values;
                })();
    
                /**
                 * TurnSide enum.
                 * @name prodigy.api.v1.TurnSide
                 * @enum {number}
                 * @property {number} TURN_SIDE_UNSPECIFIED=0 TURN_SIDE_UNSPECIFIED value
                 * @property {number} TURN_SIDE_LEFT=1 TURN_SIDE_LEFT value
                 * @property {number} TURN_SIDE_RIGHT=2 TURN_SIDE_RIGHT value
                 */
                v1.TurnSide = (function() {
                    var valuesById = {}, values = Object.create(valuesById);
                    values[valuesById[0] = "TURN_SIDE_UNSPECIFIED"] = 0;
                    values[valuesById[1] = "TURN_SIDE_LEFT"] = 1;
                    values[valuesById[2] = "TURN_SIDE_RIGHT"] = 2;
                    return values;
                })();
    
                v1.NavigationStatus = (function() {
    
                    /**
                     * Properties of a NavigationStatus.
                     * @memberof prodigy.api.v1
                     * @interface INavigationStatus
                     * @property {boolean|null} [navActive] NavigationStatus navActive
                     * @property {string|null} [roadName] NavigationStatus roadName
                     * @property {prodigy.api.v1.ManeuverType|null} [maneuver] NavigationStatus maneuver
                     * @property {prodigy.api.v1.TurnSide|null} [turnSide] NavigationStatus turnSide
                     * @property {number|null} [distanceMeters] NavigationStatus distanceMeters
                     * @property {string|null} [formattedDistance] NavigationStatus formattedDistance
                     */
    
                    /**
                     * Constructs a new NavigationStatus.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a NavigationStatus.
                     * @implements INavigationStatus
                     * @constructor
                     * @param {prodigy.api.v1.INavigationStatus=} [properties] Properties to set
                     */
                    function NavigationStatus(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * NavigationStatus navActive.
                     * @member {boolean} navActive
                     * @memberof prodigy.api.v1.NavigationStatus
                     * @instance
                     */
                    NavigationStatus.prototype.navActive = false;
    
                    /**
                     * NavigationStatus roadName.
                     * @member {string} roadName
                     * @memberof prodigy.api.v1.NavigationStatus
                     * @instance
                     */
                    NavigationStatus.prototype.roadName = "";
    
                    /**
                     * NavigationStatus maneuver.
                     * @member {prodigy.api.v1.ManeuverType} maneuver
                     * @memberof prodigy.api.v1.NavigationStatus
                     * @instance
                     */
                    NavigationStatus.prototype.maneuver = 0;
    
                    /**
                     * NavigationStatus turnSide.
                     * @member {prodigy.api.v1.TurnSide} turnSide
                     * @memberof prodigy.api.v1.NavigationStatus
                     * @instance
                     */
                    NavigationStatus.prototype.turnSide = 0;
    
                    /**
                     * NavigationStatus distanceMeters.
                     * @member {number} distanceMeters
                     * @memberof prodigy.api.v1.NavigationStatus
                     * @instance
                     */
                    NavigationStatus.prototype.distanceMeters = 0;
    
                    /**
                     * NavigationStatus formattedDistance.
                     * @member {string} formattedDistance
                     * @memberof prodigy.api.v1.NavigationStatus
                     * @instance
                     */
                    NavigationStatus.prototype.formattedDistance = "";
    
                    /**
                     * Creates a new NavigationStatus instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.NavigationStatus
                     * @static
                     * @param {prodigy.api.v1.INavigationStatus=} [properties] Properties to set
                     * @returns {prodigy.api.v1.NavigationStatus} NavigationStatus instance
                     */
                    NavigationStatus.create = function create(properties) {
                        return new NavigationStatus(properties);
                    };
    
                    /**
                     * Encodes the specified NavigationStatus message. Does not implicitly {@link prodigy.api.v1.NavigationStatus.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.NavigationStatus
                     * @static
                     * @param {prodigy.api.v1.INavigationStatus} message NavigationStatus message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    NavigationStatus.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.navActive != null && Object.hasOwnProperty.call(message, "navActive"))
                            writer.uint32(/* id 1, wireType 0 =*/8).bool(message.navActive);
                        if (message.roadName != null && Object.hasOwnProperty.call(message, "roadName"))
                            writer.uint32(/* id 2, wireType 2 =*/18).string(message.roadName);
                        if (message.maneuver != null && Object.hasOwnProperty.call(message, "maneuver"))
                            writer.uint32(/* id 3, wireType 0 =*/24).int32(message.maneuver);
                        if (message.turnSide != null && Object.hasOwnProperty.call(message, "turnSide"))
                            writer.uint32(/* id 4, wireType 0 =*/32).int32(message.turnSide);
                        if (message.distanceMeters != null && Object.hasOwnProperty.call(message, "distanceMeters"))
                            writer.uint32(/* id 5, wireType 0 =*/40).int32(message.distanceMeters);
                        if (message.formattedDistance != null && Object.hasOwnProperty.call(message, "formattedDistance"))
                            writer.uint32(/* id 6, wireType 2 =*/50).string(message.formattedDistance);
                        return writer;
                    };
    
                    /**
                     * Decodes a NavigationStatus message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.NavigationStatus
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.NavigationStatus} NavigationStatus
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    NavigationStatus.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.NavigationStatus();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.navActive = reader.bool();
                                    break;
                                }
                            case 2: {
                                    message.roadName = reader.string();
                                    break;
                                }
                            case 3: {
                                    message.maneuver = reader.int32();
                                    break;
                                }
                            case 4: {
                                    message.turnSide = reader.int32();
                                    break;
                                }
                            case 5: {
                                    message.distanceMeters = reader.int32();
                                    break;
                                }
                            case 6: {
                                    message.formattedDistance = reader.string();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a NavigationStatus message.
                     * @function verify
                     * @memberof prodigy.api.v1.NavigationStatus
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    NavigationStatus.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.navActive != null && Object.hasOwnProperty.call(message, "navActive"))
                            if (typeof message.navActive !== "boolean")
                                return "navActive: boolean expected";
                        if (message.roadName != null && Object.hasOwnProperty.call(message, "roadName"))
                            if (!$util.isString(message.roadName))
                                return "roadName: string expected";
                        if (message.maneuver != null && Object.hasOwnProperty.call(message, "maneuver"))
                            switch (message.maneuver) {
                            default:
                                return "maneuver: enum value expected";
                            case 0:
                            case 1:
                            case 2:
                            case 3:
                            case 4:
                            case 5:
                            case 6:
                            case 7:
                            case 8:
                            case 9:
                            case 10:
                            case 11:
                            case 12:
                            case 13:
                            case 14:
                            case 15:
                            case 16:
                            case 17:
                            case 18:
                                break;
                            }
                        if (message.turnSide != null && Object.hasOwnProperty.call(message, "turnSide"))
                            switch (message.turnSide) {
                            default:
                                return "turnSide: enum value expected";
                            case 0:
                            case 1:
                            case 2:
                                break;
                            }
                        if (message.distanceMeters != null && Object.hasOwnProperty.call(message, "distanceMeters"))
                            if (!$util.isInteger(message.distanceMeters))
                                return "distanceMeters: integer expected";
                        if (message.formattedDistance != null && Object.hasOwnProperty.call(message, "formattedDistance"))
                            if (!$util.isString(message.formattedDistance))
                                return "formattedDistance: string expected";
                        return null;
                    };
    
                    /**
                     * Creates a NavigationStatus message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.NavigationStatus
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.NavigationStatus} NavigationStatus
                     */
                    NavigationStatus.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.NavigationStatus)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.NavigationStatus: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.NavigationStatus();
                        if (object.navActive != null)
                            message.navActive = Boolean(object.navActive);
                        if (object.roadName != null)
                            message.roadName = String(object.roadName);
                        switch (object.maneuver) {
                        default:
                            if (typeof object.maneuver === "number") {
                                message.maneuver = object.maneuver;
                                break;
                            }
                            break;
                        case "MANEUVER_TYPE_UNSPECIFIED":
                        case 0:
                            message.maneuver = 0;
                            break;
                        case "MANEUVER_TYPE_OTHER":
                        case 1:
                            message.maneuver = 1;
                            break;
                        case "MANEUVER_TYPE_DEPART":
                        case 2:
                            message.maneuver = 2;
                            break;
                        case "MANEUVER_TYPE_STRAIGHT":
                        case 3:
                            message.maneuver = 3;
                            break;
                        case "MANEUVER_TYPE_KEEP":
                        case 4:
                            message.maneuver = 4;
                            break;
                        case "MANEUVER_TYPE_TURN":
                        case 5:
                            message.maneuver = 5;
                            break;
                        case "MANEUVER_TYPE_SHARP_TURN":
                        case 6:
                            message.maneuver = 6;
                            break;
                        case "MANEUVER_TYPE_SLIGHT_TURN":
                        case 7:
                            message.maneuver = 7;
                            break;
                        case "MANEUVER_TYPE_U_TURN":
                        case 8:
                            message.maneuver = 8;
                            break;
                        case "MANEUVER_TYPE_ON_RAMP":
                        case 9:
                            message.maneuver = 9;
                            break;
                        case "MANEUVER_TYPE_OFF_RAMP":
                        case 10:
                            message.maneuver = 10;
                            break;
                        case "MANEUVER_TYPE_FORK":
                        case 11:
                            message.maneuver = 11;
                            break;
                        case "MANEUVER_TYPE_MERGE":
                        case 12:
                            message.maneuver = 12;
                            break;
                        case "MANEUVER_TYPE_ROUNDABOUT_ENTER":
                        case 13:
                            message.maneuver = 13;
                            break;
                        case "MANEUVER_TYPE_ROUNDABOUT_EXIT":
                        case 14:
                            message.maneuver = 14;
                            break;
                        case "MANEUVER_TYPE_ROUNDABOUT_ENTER_AND_EXIT":
                        case 15:
                            message.maneuver = 15;
                            break;
                        case "MANEUVER_TYPE_FERRY":
                        case 16:
                            message.maneuver = 16;
                            break;
                        case "MANEUVER_TYPE_DESTINATION":
                        case 17:
                            message.maneuver = 17;
                            break;
                        case "MANEUVER_TYPE_NAME_CHANGE":
                        case 18:
                            message.maneuver = 18;
                            break;
                        }
                        switch (object.turnSide) {
                        default:
                            if (typeof object.turnSide === "number") {
                                message.turnSide = object.turnSide;
                                break;
                            }
                            break;
                        case "TURN_SIDE_UNSPECIFIED":
                        case 0:
                            message.turnSide = 0;
                            break;
                        case "TURN_SIDE_LEFT":
                        case 1:
                            message.turnSide = 1;
                            break;
                        case "TURN_SIDE_RIGHT":
                        case 2:
                            message.turnSide = 2;
                            break;
                        }
                        if (object.distanceMeters != null)
                            message.distanceMeters = object.distanceMeters | 0;
                        if (object.formattedDistance != null)
                            message.formattedDistance = String(object.formattedDistance);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a NavigationStatus message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.NavigationStatus
                     * @static
                     * @param {prodigy.api.v1.NavigationStatus} message NavigationStatus
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    NavigationStatus.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.navActive = false;
                            object.roadName = "";
                            object.maneuver = options.enums === String ? "MANEUVER_TYPE_UNSPECIFIED" : 0;
                            object.turnSide = options.enums === String ? "TURN_SIDE_UNSPECIFIED" : 0;
                            object.distanceMeters = 0;
                            object.formattedDistance = "";
                        }
                        if (message.navActive != null && Object.hasOwnProperty.call(message, "navActive"))
                            object.navActive = message.navActive;
                        if (message.roadName != null && Object.hasOwnProperty.call(message, "roadName"))
                            object.roadName = message.roadName;
                        if (message.maneuver != null && Object.hasOwnProperty.call(message, "maneuver"))
                            object.maneuver = options.enums === String ? $root.prodigy.api.v1.ManeuverType[message.maneuver] === undefined ? message.maneuver : $root.prodigy.api.v1.ManeuverType[message.maneuver] : message.maneuver;
                        if (message.turnSide != null && Object.hasOwnProperty.call(message, "turnSide"))
                            object.turnSide = options.enums === String ? $root.prodigy.api.v1.TurnSide[message.turnSide] === undefined ? message.turnSide : $root.prodigy.api.v1.TurnSide[message.turnSide] : message.turnSide;
                        if (message.distanceMeters != null && Object.hasOwnProperty.call(message, "distanceMeters"))
                            object.distanceMeters = message.distanceMeters;
                        if (message.formattedDistance != null && Object.hasOwnProperty.call(message, "formattedDistance"))
                            object.formattedDistance = message.formattedDistance;
                        return object;
                    };
    
                    /**
                     * Converts this NavigationStatus to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.NavigationStatus
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    NavigationStatus.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for NavigationStatus
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.NavigationStatus
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    NavigationStatus.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.NavigationStatus";
                    };
    
                    return NavigationStatus;
                })();
    
                /**
                 * ProjectionState enum.
                 * @name prodigy.api.v1.ProjectionState
                 * @enum {number}
                 * @property {number} PROJECTION_STATE_UNSPECIFIED=0 PROJECTION_STATE_UNSPECIFIED value
                 * @property {number} PROJECTION_STATE_DISCONNECTED=1 PROJECTION_STATE_DISCONNECTED value
                 * @property {number} PROJECTION_STATE_WAITING_FOR_DEVICE=2 PROJECTION_STATE_WAITING_FOR_DEVICE value
                 * @property {number} PROJECTION_STATE_CONNECTING=3 PROJECTION_STATE_CONNECTING value
                 * @property {number} PROJECTION_STATE_PROJECTING=4 PROJECTION_STATE_PROJECTING value
                 * @property {number} PROJECTION_STATE_BACKGROUNDED=5 PROJECTION_STATE_BACKGROUNDED value
                 */
                v1.ProjectionState = (function() {
                    var valuesById = {}, values = Object.create(valuesById);
                    values[valuesById[0] = "PROJECTION_STATE_UNSPECIFIED"] = 0;
                    values[valuesById[1] = "PROJECTION_STATE_DISCONNECTED"] = 1;
                    values[valuesById[2] = "PROJECTION_STATE_WAITING_FOR_DEVICE"] = 2;
                    values[valuesById[3] = "PROJECTION_STATE_CONNECTING"] = 3;
                    values[valuesById[4] = "PROJECTION_STATE_PROJECTING"] = 4;
                    values[valuesById[5] = "PROJECTION_STATE_BACKGROUNDED"] = 5;
                    return values;
                })();
    
                v1.ProjectionStatus = (function() {
    
                    /**
                     * Properties of a ProjectionStatus.
                     * @memberof prodigy.api.v1
                     * @interface IProjectionStatus
                     * @property {prodigy.api.v1.ProjectionState|null} [state] ProjectionStatus state
                     * @property {string|null} [statusMessage] ProjectionStatus statusMessage
                     */
    
                    /**
                     * Constructs a new ProjectionStatus.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a ProjectionStatus.
                     * @implements IProjectionStatus
                     * @constructor
                     * @param {prodigy.api.v1.IProjectionStatus=} [properties] Properties to set
                     */
                    function ProjectionStatus(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * ProjectionStatus state.
                     * @member {prodigy.api.v1.ProjectionState} state
                     * @memberof prodigy.api.v1.ProjectionStatus
                     * @instance
                     */
                    ProjectionStatus.prototype.state = 0;
    
                    /**
                     * ProjectionStatus statusMessage.
                     * @member {string} statusMessage
                     * @memberof prodigy.api.v1.ProjectionStatus
                     * @instance
                     */
                    ProjectionStatus.prototype.statusMessage = "";
    
                    /**
                     * Creates a new ProjectionStatus instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.ProjectionStatus
                     * @static
                     * @param {prodigy.api.v1.IProjectionStatus=} [properties] Properties to set
                     * @returns {prodigy.api.v1.ProjectionStatus} ProjectionStatus instance
                     */
                    ProjectionStatus.create = function create(properties) {
                        return new ProjectionStatus(properties);
                    };
    
                    /**
                     * Encodes the specified ProjectionStatus message. Does not implicitly {@link prodigy.api.v1.ProjectionStatus.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.ProjectionStatus
                     * @static
                     * @param {prodigy.api.v1.IProjectionStatus} message ProjectionStatus message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    ProjectionStatus.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.state != null && Object.hasOwnProperty.call(message, "state"))
                            writer.uint32(/* id 1, wireType 0 =*/8).int32(message.state);
                        if (message.statusMessage != null && Object.hasOwnProperty.call(message, "statusMessage"))
                            writer.uint32(/* id 2, wireType 2 =*/18).string(message.statusMessage);
                        return writer;
                    };
    
                    /**
                     * Decodes a ProjectionStatus message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.ProjectionStatus
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.ProjectionStatus} ProjectionStatus
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    ProjectionStatus.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.ProjectionStatus();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.state = reader.int32();
                                    break;
                                }
                            case 2: {
                                    message.statusMessage = reader.string();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a ProjectionStatus message.
                     * @function verify
                     * @memberof prodigy.api.v1.ProjectionStatus
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    ProjectionStatus.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.state != null && Object.hasOwnProperty.call(message, "state"))
                            switch (message.state) {
                            default:
                                return "state: enum value expected";
                            case 0:
                            case 1:
                            case 2:
                            case 3:
                            case 4:
                            case 5:
                                break;
                            }
                        if (message.statusMessage != null && Object.hasOwnProperty.call(message, "statusMessage"))
                            if (!$util.isString(message.statusMessage))
                                return "statusMessage: string expected";
                        return null;
                    };
    
                    /**
                     * Creates a ProjectionStatus message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.ProjectionStatus
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.ProjectionStatus} ProjectionStatus
                     */
                    ProjectionStatus.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.ProjectionStatus)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.ProjectionStatus: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.ProjectionStatus();
                        switch (object.state) {
                        default:
                            if (typeof object.state === "number") {
                                message.state = object.state;
                                break;
                            }
                            break;
                        case "PROJECTION_STATE_UNSPECIFIED":
                        case 0:
                            message.state = 0;
                            break;
                        case "PROJECTION_STATE_DISCONNECTED":
                        case 1:
                            message.state = 1;
                            break;
                        case "PROJECTION_STATE_WAITING_FOR_DEVICE":
                        case 2:
                            message.state = 2;
                            break;
                        case "PROJECTION_STATE_CONNECTING":
                        case 3:
                            message.state = 3;
                            break;
                        case "PROJECTION_STATE_PROJECTING":
                        case 4:
                            message.state = 4;
                            break;
                        case "PROJECTION_STATE_BACKGROUNDED":
                        case 5:
                            message.state = 5;
                            break;
                        }
                        if (object.statusMessage != null)
                            message.statusMessage = String(object.statusMessage);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a ProjectionStatus message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.ProjectionStatus
                     * @static
                     * @param {prodigy.api.v1.ProjectionStatus} message ProjectionStatus
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    ProjectionStatus.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.state = options.enums === String ? "PROJECTION_STATE_UNSPECIFIED" : 0;
                            object.statusMessage = "";
                        }
                        if (message.state != null && Object.hasOwnProperty.call(message, "state"))
                            object.state = options.enums === String ? $root.prodigy.api.v1.ProjectionState[message.state] === undefined ? message.state : $root.prodigy.api.v1.ProjectionState[message.state] : message.state;
                        if (message.statusMessage != null && Object.hasOwnProperty.call(message, "statusMessage"))
                            object.statusMessage = message.statusMessage;
                        return object;
                    };
    
                    /**
                     * Converts this ProjectionStatus to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.ProjectionStatus
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    ProjectionStatus.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for ProjectionStatus
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.ProjectionStatus
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    ProjectionStatus.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.ProjectionStatus";
                    };
    
                    return ProjectionStatus;
                })();
    
                /**
                 * CallState enum.
                 * @name prodigy.api.v1.CallState
                 * @enum {number}
                 * @property {number} CALL_STATE_UNSPECIFIED=0 CALL_STATE_UNSPECIFIED value
                 * @property {number} CALL_STATE_INCOMING=1 CALL_STATE_INCOMING value
                 * @property {number} CALL_STATE_DIALING=2 CALL_STATE_DIALING value
                 * @property {number} CALL_STATE_ALERTING=3 CALL_STATE_ALERTING value
                 * @property {number} CALL_STATE_ACTIVE=4 CALL_STATE_ACTIVE value
                 * @property {number} CALL_STATE_HELD=5 CALL_STATE_HELD value
                 * @property {number} CALL_STATE_WAITING=6 CALL_STATE_WAITING value
                 */
                v1.CallState = (function() {
                    var valuesById = {}, values = Object.create(valuesById);
                    values[valuesById[0] = "CALL_STATE_UNSPECIFIED"] = 0;
                    values[valuesById[1] = "CALL_STATE_INCOMING"] = 1;
                    values[valuesById[2] = "CALL_STATE_DIALING"] = 2;
                    values[valuesById[3] = "CALL_STATE_ALERTING"] = 3;
                    values[valuesById[4] = "CALL_STATE_ACTIVE"] = 4;
                    values[valuesById[5] = "CALL_STATE_HELD"] = 5;
                    values[valuesById[6] = "CALL_STATE_WAITING"] = 6;
                    return values;
                })();
    
                v1.Call = (function() {
    
                    /**
                     * Properties of a Call.
                     * @memberof prodigy.api.v1
                     * @interface ICall
                     * @property {prodigy.api.v1.CallState|null} [state] Call state
                     * @property {string|null} [lineIdentification] Call lineIdentification
                     * @property {string|null} [displayName] Call displayName
                     * @property {number|Long|null} [startedAtUnixMs] Call startedAtUnixMs
                     */
    
                    /**
                     * Constructs a new Call.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a Call.
                     * @implements ICall
                     * @constructor
                     * @param {prodigy.api.v1.ICall=} [properties] Properties to set
                     */
                    function Call(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * Call state.
                     * @member {prodigy.api.v1.CallState} state
                     * @memberof prodigy.api.v1.Call
                     * @instance
                     */
                    Call.prototype.state = 0;
    
                    /**
                     * Call lineIdentification.
                     * @member {string} lineIdentification
                     * @memberof prodigy.api.v1.Call
                     * @instance
                     */
                    Call.prototype.lineIdentification = "";
    
                    /**
                     * Call displayName.
                     * @member {string} displayName
                     * @memberof prodigy.api.v1.Call
                     * @instance
                     */
                    Call.prototype.displayName = "";
    
                    /**
                     * Call startedAtUnixMs.
                     * @member {number|Long} startedAtUnixMs
                     * @memberof prodigy.api.v1.Call
                     * @instance
                     */
                    Call.prototype.startedAtUnixMs = $util.Long ? $util.Long.fromBits(0,0,false) : 0;
    
                    /**
                     * Creates a new Call instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.Call
                     * @static
                     * @param {prodigy.api.v1.ICall=} [properties] Properties to set
                     * @returns {prodigy.api.v1.Call} Call instance
                     */
                    Call.create = function create(properties) {
                        return new Call(properties);
                    };
    
                    /**
                     * Encodes the specified Call message. Does not implicitly {@link prodigy.api.v1.Call.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.Call
                     * @static
                     * @param {prodigy.api.v1.ICall} message Call message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    Call.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.state != null && Object.hasOwnProperty.call(message, "state"))
                            writer.uint32(/* id 1, wireType 0 =*/8).int32(message.state);
                        if (message.lineIdentification != null && Object.hasOwnProperty.call(message, "lineIdentification"))
                            writer.uint32(/* id 2, wireType 2 =*/18).string(message.lineIdentification);
                        if (message.displayName != null && Object.hasOwnProperty.call(message, "displayName"))
                            writer.uint32(/* id 3, wireType 2 =*/26).string(message.displayName);
                        if (message.startedAtUnixMs != null && Object.hasOwnProperty.call(message, "startedAtUnixMs"))
                            writer.uint32(/* id 4, wireType 0 =*/32).int64(message.startedAtUnixMs);
                        return writer;
                    };
    
                    /**
                     * Decodes a Call message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.Call
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.Call} Call
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    Call.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.Call();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.state = reader.int32();
                                    break;
                                }
                            case 2: {
                                    message.lineIdentification = reader.string();
                                    break;
                                }
                            case 3: {
                                    message.displayName = reader.string();
                                    break;
                                }
                            case 4: {
                                    message.startedAtUnixMs = reader.int64();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a Call message.
                     * @function verify
                     * @memberof prodigy.api.v1.Call
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    Call.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.state != null && Object.hasOwnProperty.call(message, "state"))
                            switch (message.state) {
                            default:
                                return "state: enum value expected";
                            case 0:
                            case 1:
                            case 2:
                            case 3:
                            case 4:
                            case 5:
                            case 6:
                                break;
                            }
                        if (message.lineIdentification != null && Object.hasOwnProperty.call(message, "lineIdentification"))
                            if (!$util.isString(message.lineIdentification))
                                return "lineIdentification: string expected";
                        if (message.displayName != null && Object.hasOwnProperty.call(message, "displayName"))
                            if (!$util.isString(message.displayName))
                                return "displayName: string expected";
                        if (message.startedAtUnixMs != null && Object.hasOwnProperty.call(message, "startedAtUnixMs"))
                            if (!$util.isInteger(message.startedAtUnixMs) && !(message.startedAtUnixMs && $util.isInteger(message.startedAtUnixMs.low) && $util.isInteger(message.startedAtUnixMs.high)))
                                return "startedAtUnixMs: integer|Long expected";
                        return null;
                    };
    
                    /**
                     * Creates a Call message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.Call
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.Call} Call
                     */
                    Call.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.Call)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.Call: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.Call();
                        switch (object.state) {
                        default:
                            if (typeof object.state === "number") {
                                message.state = object.state;
                                break;
                            }
                            break;
                        case "CALL_STATE_UNSPECIFIED":
                        case 0:
                            message.state = 0;
                            break;
                        case "CALL_STATE_INCOMING":
                        case 1:
                            message.state = 1;
                            break;
                        case "CALL_STATE_DIALING":
                        case 2:
                            message.state = 2;
                            break;
                        case "CALL_STATE_ALERTING":
                        case 3:
                            message.state = 3;
                            break;
                        case "CALL_STATE_ACTIVE":
                        case 4:
                            message.state = 4;
                            break;
                        case "CALL_STATE_HELD":
                        case 5:
                            message.state = 5;
                            break;
                        case "CALL_STATE_WAITING":
                        case 6:
                            message.state = 6;
                            break;
                        }
                        if (object.lineIdentification != null)
                            message.lineIdentification = String(object.lineIdentification);
                        if (object.displayName != null)
                            message.displayName = String(object.displayName);
                        if (object.startedAtUnixMs != null)
                            if ($util.Long)
                                message.startedAtUnixMs = $util.Long.fromValue(object.startedAtUnixMs, false);
                            else if (typeof object.startedAtUnixMs === "string")
                                message.startedAtUnixMs = parseInt(object.startedAtUnixMs, 10);
                            else if (typeof object.startedAtUnixMs === "number")
                                message.startedAtUnixMs = object.startedAtUnixMs;
                            else if (typeof object.startedAtUnixMs === "object")
                                message.startedAtUnixMs = new $util.LongBits(object.startedAtUnixMs.low >>> 0, object.startedAtUnixMs.high >>> 0).toNumber();
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a Call message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.Call
                     * @static
                     * @param {prodigy.api.v1.Call} message Call
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    Call.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.state = options.enums === String ? "CALL_STATE_UNSPECIFIED" : 0;
                            object.lineIdentification = "";
                            object.displayName = "";
                            if ($util.Long) {
                                var long = new $util.Long(0, 0, false);
                                object.startedAtUnixMs = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : typeof BigInt !== "undefined" && options.longs === BigInt ? long.toBigInt() : long;
                            } else
                                object.startedAtUnixMs = options.longs === String ? "0" : typeof BigInt !== "undefined" && options.longs === BigInt ? BigInt("0") : 0;
                        }
                        if (message.state != null && Object.hasOwnProperty.call(message, "state"))
                            object.state = options.enums === String ? $root.prodigy.api.v1.CallState[message.state] === undefined ? message.state : $root.prodigy.api.v1.CallState[message.state] : message.state;
                        if (message.lineIdentification != null && Object.hasOwnProperty.call(message, "lineIdentification"))
                            object.lineIdentification = message.lineIdentification;
                        if (message.displayName != null && Object.hasOwnProperty.call(message, "displayName"))
                            object.displayName = message.displayName;
                        if (message.startedAtUnixMs != null && Object.hasOwnProperty.call(message, "startedAtUnixMs"))
                            if (typeof BigInt !== "undefined" && options.longs === BigInt)
                                object.startedAtUnixMs = typeof message.startedAtUnixMs === "number" ? BigInt(message.startedAtUnixMs) : $util.Long.fromBits(message.startedAtUnixMs.low >>> 0, message.startedAtUnixMs.high >>> 0, false).toBigInt();
                            else if (typeof message.startedAtUnixMs === "number")
                                object.startedAtUnixMs = options.longs === String ? String(message.startedAtUnixMs) : message.startedAtUnixMs;
                            else
                                object.startedAtUnixMs = options.longs === String ? $util.Long.prototype.toString.call(message.startedAtUnixMs) : options.longs === Number ? new $util.LongBits(message.startedAtUnixMs.low >>> 0, message.startedAtUnixMs.high >>> 0).toNumber() : message.startedAtUnixMs;
                        return object;
                    };
    
                    /**
                     * Converts this Call to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.Call
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    Call.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for Call
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.Call
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    Call.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.Call";
                    };
    
                    return Call;
                })();
    
                v1.PhoneCapabilities = (function() {
    
                    /**
                     * Properties of a PhoneCapabilities.
                     * @memberof prodigy.api.v1
                     * @interface IPhoneCapabilities
                     * @property {boolean|null} [canDial] PhoneCapabilities canDial
                     * @property {boolean|null} [canAnswer] PhoneCapabilities canAnswer
                     * @property {boolean|null} [canHangup] PhoneCapabilities canHangup
                     * @property {boolean|null} [canSendDtmf] PhoneCapabilities canSendDtmf
                     * @property {boolean|null} [canHoldSwap] PhoneCapabilities canHoldSwap
                     * @property {boolean|null} [canMultiparty] PhoneCapabilities canMultiparty
                     */
    
                    /**
                     * Constructs a new PhoneCapabilities.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a PhoneCapabilities.
                     * @implements IPhoneCapabilities
                     * @constructor
                     * @param {prodigy.api.v1.IPhoneCapabilities=} [properties] Properties to set
                     */
                    function PhoneCapabilities(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * PhoneCapabilities canDial.
                     * @member {boolean} canDial
                     * @memberof prodigy.api.v1.PhoneCapabilities
                     * @instance
                     */
                    PhoneCapabilities.prototype.canDial = false;
    
                    /**
                     * PhoneCapabilities canAnswer.
                     * @member {boolean} canAnswer
                     * @memberof prodigy.api.v1.PhoneCapabilities
                     * @instance
                     */
                    PhoneCapabilities.prototype.canAnswer = false;
    
                    /**
                     * PhoneCapabilities canHangup.
                     * @member {boolean} canHangup
                     * @memberof prodigy.api.v1.PhoneCapabilities
                     * @instance
                     */
                    PhoneCapabilities.prototype.canHangup = false;
    
                    /**
                     * PhoneCapabilities canSendDtmf.
                     * @member {boolean} canSendDtmf
                     * @memberof prodigy.api.v1.PhoneCapabilities
                     * @instance
                     */
                    PhoneCapabilities.prototype.canSendDtmf = false;
    
                    /**
                     * PhoneCapabilities canHoldSwap.
                     * @member {boolean} canHoldSwap
                     * @memberof prodigy.api.v1.PhoneCapabilities
                     * @instance
                     */
                    PhoneCapabilities.prototype.canHoldSwap = false;
    
                    /**
                     * PhoneCapabilities canMultiparty.
                     * @member {boolean} canMultiparty
                     * @memberof prodigy.api.v1.PhoneCapabilities
                     * @instance
                     */
                    PhoneCapabilities.prototype.canMultiparty = false;
    
                    /**
                     * Creates a new PhoneCapabilities instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.PhoneCapabilities
                     * @static
                     * @param {prodigy.api.v1.IPhoneCapabilities=} [properties] Properties to set
                     * @returns {prodigy.api.v1.PhoneCapabilities} PhoneCapabilities instance
                     */
                    PhoneCapabilities.create = function create(properties) {
                        return new PhoneCapabilities(properties);
                    };
    
                    /**
                     * Encodes the specified PhoneCapabilities message. Does not implicitly {@link prodigy.api.v1.PhoneCapabilities.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.PhoneCapabilities
                     * @static
                     * @param {prodigy.api.v1.IPhoneCapabilities} message PhoneCapabilities message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    PhoneCapabilities.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.canDial != null && Object.hasOwnProperty.call(message, "canDial"))
                            writer.uint32(/* id 1, wireType 0 =*/8).bool(message.canDial);
                        if (message.canAnswer != null && Object.hasOwnProperty.call(message, "canAnswer"))
                            writer.uint32(/* id 2, wireType 0 =*/16).bool(message.canAnswer);
                        if (message.canHangup != null && Object.hasOwnProperty.call(message, "canHangup"))
                            writer.uint32(/* id 3, wireType 0 =*/24).bool(message.canHangup);
                        if (message.canSendDtmf != null && Object.hasOwnProperty.call(message, "canSendDtmf"))
                            writer.uint32(/* id 4, wireType 0 =*/32).bool(message.canSendDtmf);
                        if (message.canHoldSwap != null && Object.hasOwnProperty.call(message, "canHoldSwap"))
                            writer.uint32(/* id 5, wireType 0 =*/40).bool(message.canHoldSwap);
                        if (message.canMultiparty != null && Object.hasOwnProperty.call(message, "canMultiparty"))
                            writer.uint32(/* id 6, wireType 0 =*/48).bool(message.canMultiparty);
                        return writer;
                    };
    
                    /**
                     * Decodes a PhoneCapabilities message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.PhoneCapabilities
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.PhoneCapabilities} PhoneCapabilities
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    PhoneCapabilities.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.PhoneCapabilities();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.canDial = reader.bool();
                                    break;
                                }
                            case 2: {
                                    message.canAnswer = reader.bool();
                                    break;
                                }
                            case 3: {
                                    message.canHangup = reader.bool();
                                    break;
                                }
                            case 4: {
                                    message.canSendDtmf = reader.bool();
                                    break;
                                }
                            case 5: {
                                    message.canHoldSwap = reader.bool();
                                    break;
                                }
                            case 6: {
                                    message.canMultiparty = reader.bool();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a PhoneCapabilities message.
                     * @function verify
                     * @memberof prodigy.api.v1.PhoneCapabilities
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    PhoneCapabilities.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.canDial != null && Object.hasOwnProperty.call(message, "canDial"))
                            if (typeof message.canDial !== "boolean")
                                return "canDial: boolean expected";
                        if (message.canAnswer != null && Object.hasOwnProperty.call(message, "canAnswer"))
                            if (typeof message.canAnswer !== "boolean")
                                return "canAnswer: boolean expected";
                        if (message.canHangup != null && Object.hasOwnProperty.call(message, "canHangup"))
                            if (typeof message.canHangup !== "boolean")
                                return "canHangup: boolean expected";
                        if (message.canSendDtmf != null && Object.hasOwnProperty.call(message, "canSendDtmf"))
                            if (typeof message.canSendDtmf !== "boolean")
                                return "canSendDtmf: boolean expected";
                        if (message.canHoldSwap != null && Object.hasOwnProperty.call(message, "canHoldSwap"))
                            if (typeof message.canHoldSwap !== "boolean")
                                return "canHoldSwap: boolean expected";
                        if (message.canMultiparty != null && Object.hasOwnProperty.call(message, "canMultiparty"))
                            if (typeof message.canMultiparty !== "boolean")
                                return "canMultiparty: boolean expected";
                        return null;
                    };
    
                    /**
                     * Creates a PhoneCapabilities message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.PhoneCapabilities
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.PhoneCapabilities} PhoneCapabilities
                     */
                    PhoneCapabilities.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.PhoneCapabilities)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.PhoneCapabilities: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.PhoneCapabilities();
                        if (object.canDial != null)
                            message.canDial = Boolean(object.canDial);
                        if (object.canAnswer != null)
                            message.canAnswer = Boolean(object.canAnswer);
                        if (object.canHangup != null)
                            message.canHangup = Boolean(object.canHangup);
                        if (object.canSendDtmf != null)
                            message.canSendDtmf = Boolean(object.canSendDtmf);
                        if (object.canHoldSwap != null)
                            message.canHoldSwap = Boolean(object.canHoldSwap);
                        if (object.canMultiparty != null)
                            message.canMultiparty = Boolean(object.canMultiparty);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a PhoneCapabilities message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.PhoneCapabilities
                     * @static
                     * @param {prodigy.api.v1.PhoneCapabilities} message PhoneCapabilities
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    PhoneCapabilities.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.canDial = false;
                            object.canAnswer = false;
                            object.canHangup = false;
                            object.canSendDtmf = false;
                            object.canHoldSwap = false;
                            object.canMultiparty = false;
                        }
                        if (message.canDial != null && Object.hasOwnProperty.call(message, "canDial"))
                            object.canDial = message.canDial;
                        if (message.canAnswer != null && Object.hasOwnProperty.call(message, "canAnswer"))
                            object.canAnswer = message.canAnswer;
                        if (message.canHangup != null && Object.hasOwnProperty.call(message, "canHangup"))
                            object.canHangup = message.canHangup;
                        if (message.canSendDtmf != null && Object.hasOwnProperty.call(message, "canSendDtmf"))
                            object.canSendDtmf = message.canSendDtmf;
                        if (message.canHoldSwap != null && Object.hasOwnProperty.call(message, "canHoldSwap"))
                            object.canHoldSwap = message.canHoldSwap;
                        if (message.canMultiparty != null && Object.hasOwnProperty.call(message, "canMultiparty"))
                            object.canMultiparty = message.canMultiparty;
                        return object;
                    };
    
                    /**
                     * Converts this PhoneCapabilities to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.PhoneCapabilities
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    PhoneCapabilities.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for PhoneCapabilities
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.PhoneCapabilities
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    PhoneCapabilities.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.PhoneCapabilities";
                    };
    
                    return PhoneCapabilities;
                })();
    
                v1.PhoneStatus = (function() {
    
                    /**
                     * Properties of a PhoneStatus.
                     * @memberof prodigy.api.v1
                     * @interface IPhoneStatus
                     * @property {boolean|null} [hfpConnected] PhoneStatus hfpConnected
                     * @property {string|null} [deviceName] PhoneStatus deviceName
                     * @property {Array.<prodigy.api.v1.ICall>|null} [calls] PhoneStatus calls
                     * @property {prodigy.api.v1.IPhoneCapabilities|null} [capabilities] PhoneStatus capabilities
                     */
    
                    /**
                     * Constructs a new PhoneStatus.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a PhoneStatus.
                     * @implements IPhoneStatus
                     * @constructor
                     * @param {prodigy.api.v1.IPhoneStatus=} [properties] Properties to set
                     */
                    function PhoneStatus(properties) {
                        this.calls = [];
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * PhoneStatus hfpConnected.
                     * @member {boolean} hfpConnected
                     * @memberof prodigy.api.v1.PhoneStatus
                     * @instance
                     */
                    PhoneStatus.prototype.hfpConnected = false;
    
                    /**
                     * PhoneStatus deviceName.
                     * @member {string} deviceName
                     * @memberof prodigy.api.v1.PhoneStatus
                     * @instance
                     */
                    PhoneStatus.prototype.deviceName = "";
    
                    /**
                     * PhoneStatus calls.
                     * @member {Array.<prodigy.api.v1.ICall>} calls
                     * @memberof prodigy.api.v1.PhoneStatus
                     * @instance
                     */
                    PhoneStatus.prototype.calls = $util.emptyArray;
    
                    /**
                     * PhoneStatus capabilities.
                     * @member {prodigy.api.v1.IPhoneCapabilities|null|undefined} capabilities
                     * @memberof prodigy.api.v1.PhoneStatus
                     * @instance
                     */
                    PhoneStatus.prototype.capabilities = null;
    
                    /**
                     * Creates a new PhoneStatus instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.PhoneStatus
                     * @static
                     * @param {prodigy.api.v1.IPhoneStatus=} [properties] Properties to set
                     * @returns {prodigy.api.v1.PhoneStatus} PhoneStatus instance
                     */
                    PhoneStatus.create = function create(properties) {
                        return new PhoneStatus(properties);
                    };
    
                    /**
                     * Encodes the specified PhoneStatus message. Does not implicitly {@link prodigy.api.v1.PhoneStatus.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.PhoneStatus
                     * @static
                     * @param {prodigy.api.v1.IPhoneStatus} message PhoneStatus message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    PhoneStatus.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.hfpConnected != null && Object.hasOwnProperty.call(message, "hfpConnected"))
                            writer.uint32(/* id 1, wireType 0 =*/8).bool(message.hfpConnected);
                        if (message.deviceName != null && Object.hasOwnProperty.call(message, "deviceName"))
                            writer.uint32(/* id 2, wireType 2 =*/18).string(message.deviceName);
                        if (message.calls != null && message.calls.length)
                            for (var i = 0; i < message.calls.length; ++i)
                                $root.prodigy.api.v1.Call.encode(message.calls[i], writer.uint32(/* id 3, wireType 2 =*/26).fork(), q + 1).ldelim();
                        if (message.capabilities != null && Object.hasOwnProperty.call(message, "capabilities"))
                            $root.prodigy.api.v1.PhoneCapabilities.encode(message.capabilities, writer.uint32(/* id 4, wireType 2 =*/34).fork(), q + 1).ldelim();
                        return writer;
                    };
    
                    /**
                     * Decodes a PhoneStatus message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.PhoneStatus
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.PhoneStatus} PhoneStatus
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    PhoneStatus.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.PhoneStatus();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.hfpConnected = reader.bool();
                                    break;
                                }
                            case 2: {
                                    message.deviceName = reader.string();
                                    break;
                                }
                            case 3: {
                                    if (!(message.calls && message.calls.length))
                                        message.calls = [];
                                    message.calls.push($root.prodigy.api.v1.Call.decode(reader, reader.uint32(), undefined, long + 1));
                                    break;
                                }
                            case 4: {
                                    message.capabilities = $root.prodigy.api.v1.PhoneCapabilities.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a PhoneStatus message.
                     * @function verify
                     * @memberof prodigy.api.v1.PhoneStatus
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    PhoneStatus.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.hfpConnected != null && Object.hasOwnProperty.call(message, "hfpConnected"))
                            if (typeof message.hfpConnected !== "boolean")
                                return "hfpConnected: boolean expected";
                        if (message.deviceName != null && Object.hasOwnProperty.call(message, "deviceName"))
                            if (!$util.isString(message.deviceName))
                                return "deviceName: string expected";
                        if (message.calls != null && Object.hasOwnProperty.call(message, "calls")) {
                            if (!Array.isArray(message.calls))
                                return "calls: array expected";
                            for (var i = 0; i < message.calls.length; ++i) {
                                var error = $root.prodigy.api.v1.Call.verify(message.calls[i], long + 1);
                                if (error)
                                    return "calls." + error;
                            }
                        }
                        if (message.capabilities != null && Object.hasOwnProperty.call(message, "capabilities")) {
                            var error = $root.prodigy.api.v1.PhoneCapabilities.verify(message.capabilities, long + 1);
                            if (error)
                                return "capabilities." + error;
                        }
                        return null;
                    };
    
                    /**
                     * Creates a PhoneStatus message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.PhoneStatus
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.PhoneStatus} PhoneStatus
                     */
                    PhoneStatus.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.PhoneStatus)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.PhoneStatus: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.PhoneStatus();
                        if (object.hfpConnected != null)
                            message.hfpConnected = Boolean(object.hfpConnected);
                        if (object.deviceName != null)
                            message.deviceName = String(object.deviceName);
                        if (object.calls) {
                            if (!Array.isArray(object.calls))
                                throw TypeError(".prodigy.api.v1.PhoneStatus.calls: array expected");
                            message.calls = [];
                            for (var i = 0; i < object.calls.length; ++i) {
                                if (!$util.isObject(object.calls[i]))
                                    throw TypeError(".prodigy.api.v1.PhoneStatus.calls: object expected");
                                message.calls[i] = $root.prodigy.api.v1.Call.fromObject(object.calls[i], long + 1);
                            }
                        }
                        if (object.capabilities != null) {
                            if (!$util.isObject(object.capabilities))
                                throw TypeError(".prodigy.api.v1.PhoneStatus.capabilities: object expected");
                            message.capabilities = $root.prodigy.api.v1.PhoneCapabilities.fromObject(object.capabilities, long + 1);
                        }
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a PhoneStatus message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.PhoneStatus
                     * @static
                     * @param {prodigy.api.v1.PhoneStatus} message PhoneStatus
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    PhoneStatus.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.arrays || options.defaults)
                            object.calls = [];
                        if (options.defaults) {
                            object.hfpConnected = false;
                            object.deviceName = "";
                            object.capabilities = null;
                        }
                        if (message.hfpConnected != null && Object.hasOwnProperty.call(message, "hfpConnected"))
                            object.hfpConnected = message.hfpConnected;
                        if (message.deviceName != null && Object.hasOwnProperty.call(message, "deviceName"))
                            object.deviceName = message.deviceName;
                        if (message.calls && message.calls.length) {
                            object.calls = [];
                            for (var j = 0; j < message.calls.length; ++j)
                                object.calls[j] = $root.prodigy.api.v1.Call.toObject(message.calls[j], options, q + 1);
                        }
                        if (message.capabilities != null && Object.hasOwnProperty.call(message, "capabilities"))
                            object.capabilities = $root.prodigy.api.v1.PhoneCapabilities.toObject(message.capabilities, options, q + 1);
                        return object;
                    };
    
                    /**
                     * Converts this PhoneStatus to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.PhoneStatus
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    PhoneStatus.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for PhoneStatus
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.PhoneStatus
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    PhoneStatus.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.PhoneStatus";
                    };
    
                    return PhoneStatus;
                })();
    
                /**
                 * PhoneCommandResult enum.
                 * @name prodigy.api.v1.PhoneCommandResult
                 * @enum {number}
                 * @property {number} PHONE_COMMAND_RESULT_UNSPECIFIED=0 PHONE_COMMAND_RESULT_UNSPECIFIED value
                 * @property {number} PHONE_COMMAND_RESULT_OK=1 PHONE_COMMAND_RESULT_OK value
                 * @property {number} PHONE_COMMAND_RESULT_UNAVAILABLE=2 PHONE_COMMAND_RESULT_UNAVAILABLE value
                 * @property {number} PHONE_COMMAND_RESULT_FAILED=3 PHONE_COMMAND_RESULT_FAILED value
                 */
                v1.PhoneCommandResult = (function() {
                    var valuesById = {}, values = Object.create(valuesById);
                    values[valuesById[0] = "PHONE_COMMAND_RESULT_UNSPECIFIED"] = 0;
                    values[valuesById[1] = "PHONE_COMMAND_RESULT_OK"] = 1;
                    values[valuesById[2] = "PHONE_COMMAND_RESULT_UNAVAILABLE"] = 2;
                    values[valuesById[3] = "PHONE_COMMAND_RESULT_FAILED"] = 3;
                    return values;
                })();
    
                v1.DialRequest = (function() {
    
                    /**
                     * Properties of a DialRequest.
                     * @memberof prodigy.api.v1
                     * @interface IDialRequest
                     * @property {string|null} [number] DialRequest number
                     */
    
                    /**
                     * Constructs a new DialRequest.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a DialRequest.
                     * @implements IDialRequest
                     * @constructor
                     * @param {prodigy.api.v1.IDialRequest=} [properties] Properties to set
                     */
                    function DialRequest(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * DialRequest number.
                     * @member {string} number
                     * @memberof prodigy.api.v1.DialRequest
                     * @instance
                     */
                    DialRequest.prototype.number = "";
    
                    /**
                     * Creates a new DialRequest instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.DialRequest
                     * @static
                     * @param {prodigy.api.v1.IDialRequest=} [properties] Properties to set
                     * @returns {prodigy.api.v1.DialRequest} DialRequest instance
                     */
                    DialRequest.create = function create(properties) {
                        return new DialRequest(properties);
                    };
    
                    /**
                     * Encodes the specified DialRequest message. Does not implicitly {@link prodigy.api.v1.DialRequest.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.DialRequest
                     * @static
                     * @param {prodigy.api.v1.IDialRequest} message DialRequest message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    DialRequest.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.number != null && Object.hasOwnProperty.call(message, "number"))
                            writer.uint32(/* id 1, wireType 2 =*/10).string(message.number);
                        return writer;
                    };
    
                    /**
                     * Decodes a DialRequest message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.DialRequest
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.DialRequest} DialRequest
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    DialRequest.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.DialRequest();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.number = reader.string();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a DialRequest message.
                     * @function verify
                     * @memberof prodigy.api.v1.DialRequest
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    DialRequest.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.number != null && Object.hasOwnProperty.call(message, "number"))
                            if (!$util.isString(message.number))
                                return "number: string expected";
                        return null;
                    };
    
                    /**
                     * Creates a DialRequest message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.DialRequest
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.DialRequest} DialRequest
                     */
                    DialRequest.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.DialRequest)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.DialRequest: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.DialRequest();
                        if (object.number != null)
                            message.number = String(object.number);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a DialRequest message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.DialRequest
                     * @static
                     * @param {prodigy.api.v1.DialRequest} message DialRequest
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    DialRequest.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults)
                            object.number = "";
                        if (message.number != null && Object.hasOwnProperty.call(message, "number"))
                            object.number = message.number;
                        return object;
                    };
    
                    /**
                     * Converts this DialRequest to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.DialRequest
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    DialRequest.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for DialRequest
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.DialRequest
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    DialRequest.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.DialRequest";
                    };
    
                    return DialRequest;
                })();
    
                v1.AnswerCallRequest = (function() {
    
                    /**
                     * Properties of an AnswerCallRequest.
                     * @memberof prodigy.api.v1
                     * @interface IAnswerCallRequest
                     */
    
                    /**
                     * Constructs a new AnswerCallRequest.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents an AnswerCallRequest.
                     * @implements IAnswerCallRequest
                     * @constructor
                     * @param {prodigy.api.v1.IAnswerCallRequest=} [properties] Properties to set
                     */
                    function AnswerCallRequest(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * Creates a new AnswerCallRequest instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.AnswerCallRequest
                     * @static
                     * @param {prodigy.api.v1.IAnswerCallRequest=} [properties] Properties to set
                     * @returns {prodigy.api.v1.AnswerCallRequest} AnswerCallRequest instance
                     */
                    AnswerCallRequest.create = function create(properties) {
                        return new AnswerCallRequest(properties);
                    };
    
                    /**
                     * Encodes the specified AnswerCallRequest message. Does not implicitly {@link prodigy.api.v1.AnswerCallRequest.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.AnswerCallRequest
                     * @static
                     * @param {prodigy.api.v1.IAnswerCallRequest} message AnswerCallRequest message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    AnswerCallRequest.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        return writer;
                    };
    
                    /**
                     * Decodes an AnswerCallRequest message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.AnswerCallRequest
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.AnswerCallRequest} AnswerCallRequest
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    AnswerCallRequest.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.AnswerCallRequest();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies an AnswerCallRequest message.
                     * @function verify
                     * @memberof prodigy.api.v1.AnswerCallRequest
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    AnswerCallRequest.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        return null;
                    };
    
                    /**
                     * Creates an AnswerCallRequest message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.AnswerCallRequest
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.AnswerCallRequest} AnswerCallRequest
                     */
                    AnswerCallRequest.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.AnswerCallRequest)
                            return object;
                        return new $root.prodigy.api.v1.AnswerCallRequest();
                    };
    
                    /**
                     * Creates a plain object from an AnswerCallRequest message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.AnswerCallRequest
                     * @static
                     * @param {prodigy.api.v1.AnswerCallRequest} message AnswerCallRequest
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    AnswerCallRequest.toObject = function toObject() {
                        return {};
                    };
    
                    /**
                     * Converts this AnswerCallRequest to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.AnswerCallRequest
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    AnswerCallRequest.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for AnswerCallRequest
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.AnswerCallRequest
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    AnswerCallRequest.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.AnswerCallRequest";
                    };
    
                    return AnswerCallRequest;
                })();
    
                v1.HangupRequest = (function() {
    
                    /**
                     * Properties of a HangupRequest.
                     * @memberof prodigy.api.v1
                     * @interface IHangupRequest
                     */
    
                    /**
                     * Constructs a new HangupRequest.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a HangupRequest.
                     * @implements IHangupRequest
                     * @constructor
                     * @param {prodigy.api.v1.IHangupRequest=} [properties] Properties to set
                     */
                    function HangupRequest(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * Creates a new HangupRequest instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.HangupRequest
                     * @static
                     * @param {prodigy.api.v1.IHangupRequest=} [properties] Properties to set
                     * @returns {prodigy.api.v1.HangupRequest} HangupRequest instance
                     */
                    HangupRequest.create = function create(properties) {
                        return new HangupRequest(properties);
                    };
    
                    /**
                     * Encodes the specified HangupRequest message. Does not implicitly {@link prodigy.api.v1.HangupRequest.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.HangupRequest
                     * @static
                     * @param {prodigy.api.v1.IHangupRequest} message HangupRequest message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    HangupRequest.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        return writer;
                    };
    
                    /**
                     * Decodes a HangupRequest message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.HangupRequest
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.HangupRequest} HangupRequest
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    HangupRequest.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.HangupRequest();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a HangupRequest message.
                     * @function verify
                     * @memberof prodigy.api.v1.HangupRequest
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    HangupRequest.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        return null;
                    };
    
                    /**
                     * Creates a HangupRequest message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.HangupRequest
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.HangupRequest} HangupRequest
                     */
                    HangupRequest.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.HangupRequest)
                            return object;
                        return new $root.prodigy.api.v1.HangupRequest();
                    };
    
                    /**
                     * Creates a plain object from a HangupRequest message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.HangupRequest
                     * @static
                     * @param {prodigy.api.v1.HangupRequest} message HangupRequest
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    HangupRequest.toObject = function toObject() {
                        return {};
                    };
    
                    /**
                     * Converts this HangupRequest to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.HangupRequest
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    HangupRequest.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for HangupRequest
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.HangupRequest
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    HangupRequest.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.HangupRequest";
                    };
    
                    return HangupRequest;
                })();
    
                v1.SendDtmfRequest = (function() {
    
                    /**
                     * Properties of a SendDtmfRequest.
                     * @memberof prodigy.api.v1
                     * @interface ISendDtmfRequest
                     * @property {string|null} [tones] SendDtmfRequest tones
                     */
    
                    /**
                     * Constructs a new SendDtmfRequest.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a SendDtmfRequest.
                     * @implements ISendDtmfRequest
                     * @constructor
                     * @param {prodigy.api.v1.ISendDtmfRequest=} [properties] Properties to set
                     */
                    function SendDtmfRequest(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * SendDtmfRequest tones.
                     * @member {string} tones
                     * @memberof prodigy.api.v1.SendDtmfRequest
                     * @instance
                     */
                    SendDtmfRequest.prototype.tones = "";
    
                    /**
                     * Creates a new SendDtmfRequest instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.SendDtmfRequest
                     * @static
                     * @param {prodigy.api.v1.ISendDtmfRequest=} [properties] Properties to set
                     * @returns {prodigy.api.v1.SendDtmfRequest} SendDtmfRequest instance
                     */
                    SendDtmfRequest.create = function create(properties) {
                        return new SendDtmfRequest(properties);
                    };
    
                    /**
                     * Encodes the specified SendDtmfRequest message. Does not implicitly {@link prodigy.api.v1.SendDtmfRequest.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.SendDtmfRequest
                     * @static
                     * @param {prodigy.api.v1.ISendDtmfRequest} message SendDtmfRequest message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    SendDtmfRequest.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.tones != null && Object.hasOwnProperty.call(message, "tones"))
                            writer.uint32(/* id 1, wireType 2 =*/10).string(message.tones);
                        return writer;
                    };
    
                    /**
                     * Decodes a SendDtmfRequest message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.SendDtmfRequest
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.SendDtmfRequest} SendDtmfRequest
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    SendDtmfRequest.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.SendDtmfRequest();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.tones = reader.string();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a SendDtmfRequest message.
                     * @function verify
                     * @memberof prodigy.api.v1.SendDtmfRequest
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    SendDtmfRequest.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.tones != null && Object.hasOwnProperty.call(message, "tones"))
                            if (!$util.isString(message.tones))
                                return "tones: string expected";
                        return null;
                    };
    
                    /**
                     * Creates a SendDtmfRequest message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.SendDtmfRequest
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.SendDtmfRequest} SendDtmfRequest
                     */
                    SendDtmfRequest.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.SendDtmfRequest)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.SendDtmfRequest: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.SendDtmfRequest();
                        if (object.tones != null)
                            message.tones = String(object.tones);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a SendDtmfRequest message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.SendDtmfRequest
                     * @static
                     * @param {prodigy.api.v1.SendDtmfRequest} message SendDtmfRequest
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    SendDtmfRequest.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults)
                            object.tones = "";
                        if (message.tones != null && Object.hasOwnProperty.call(message, "tones"))
                            object.tones = message.tones;
                        return object;
                    };
    
                    /**
                     * Converts this SendDtmfRequest to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.SendDtmfRequest
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    SendDtmfRequest.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for SendDtmfRequest
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.SendDtmfRequest
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    SendDtmfRequest.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.SendDtmfRequest";
                    };
    
                    return SendDtmfRequest;
                })();
    
                v1.PhoneCommandResponse = (function() {
    
                    /**
                     * Properties of a PhoneCommandResponse.
                     * @memberof prodigy.api.v1
                     * @interface IPhoneCommandResponse
                     * @property {prodigy.api.v1.PhoneCommandResult|null} [result] PhoneCommandResponse result
                     * @property {string|null} [detail] PhoneCommandResponse detail
                     */
    
                    /**
                     * Constructs a new PhoneCommandResponse.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a PhoneCommandResponse.
                     * @implements IPhoneCommandResponse
                     * @constructor
                     * @param {prodigy.api.v1.IPhoneCommandResponse=} [properties] Properties to set
                     */
                    function PhoneCommandResponse(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * PhoneCommandResponse result.
                     * @member {prodigy.api.v1.PhoneCommandResult} result
                     * @memberof prodigy.api.v1.PhoneCommandResponse
                     * @instance
                     */
                    PhoneCommandResponse.prototype.result = 0;
    
                    /**
                     * PhoneCommandResponse detail.
                     * @member {string} detail
                     * @memberof prodigy.api.v1.PhoneCommandResponse
                     * @instance
                     */
                    PhoneCommandResponse.prototype.detail = "";
    
                    /**
                     * Creates a new PhoneCommandResponse instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.PhoneCommandResponse
                     * @static
                     * @param {prodigy.api.v1.IPhoneCommandResponse=} [properties] Properties to set
                     * @returns {prodigy.api.v1.PhoneCommandResponse} PhoneCommandResponse instance
                     */
                    PhoneCommandResponse.create = function create(properties) {
                        return new PhoneCommandResponse(properties);
                    };
    
                    /**
                     * Encodes the specified PhoneCommandResponse message. Does not implicitly {@link prodigy.api.v1.PhoneCommandResponse.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.PhoneCommandResponse
                     * @static
                     * @param {prodigy.api.v1.IPhoneCommandResponse} message PhoneCommandResponse message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    PhoneCommandResponse.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.result != null && Object.hasOwnProperty.call(message, "result"))
                            writer.uint32(/* id 1, wireType 0 =*/8).int32(message.result);
                        if (message.detail != null && Object.hasOwnProperty.call(message, "detail"))
                            writer.uint32(/* id 2, wireType 2 =*/18).string(message.detail);
                        return writer;
                    };
    
                    /**
                     * Decodes a PhoneCommandResponse message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.PhoneCommandResponse
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.PhoneCommandResponse} PhoneCommandResponse
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    PhoneCommandResponse.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.PhoneCommandResponse();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.result = reader.int32();
                                    break;
                                }
                            case 2: {
                                    message.detail = reader.string();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a PhoneCommandResponse message.
                     * @function verify
                     * @memberof prodigy.api.v1.PhoneCommandResponse
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    PhoneCommandResponse.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.result != null && Object.hasOwnProperty.call(message, "result"))
                            switch (message.result) {
                            default:
                                return "result: enum value expected";
                            case 0:
                            case 1:
                            case 2:
                            case 3:
                                break;
                            }
                        if (message.detail != null && Object.hasOwnProperty.call(message, "detail"))
                            if (!$util.isString(message.detail))
                                return "detail: string expected";
                        return null;
                    };
    
                    /**
                     * Creates a PhoneCommandResponse message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.PhoneCommandResponse
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.PhoneCommandResponse} PhoneCommandResponse
                     */
                    PhoneCommandResponse.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.PhoneCommandResponse)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.PhoneCommandResponse: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.PhoneCommandResponse();
                        switch (object.result) {
                        default:
                            if (typeof object.result === "number") {
                                message.result = object.result;
                                break;
                            }
                            break;
                        case "PHONE_COMMAND_RESULT_UNSPECIFIED":
                        case 0:
                            message.result = 0;
                            break;
                        case "PHONE_COMMAND_RESULT_OK":
                        case 1:
                            message.result = 1;
                            break;
                        case "PHONE_COMMAND_RESULT_UNAVAILABLE":
                        case 2:
                            message.result = 2;
                            break;
                        case "PHONE_COMMAND_RESULT_FAILED":
                        case 3:
                            message.result = 3;
                            break;
                        }
                        if (object.detail != null)
                            message.detail = String(object.detail);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a PhoneCommandResponse message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.PhoneCommandResponse
                     * @static
                     * @param {prodigy.api.v1.PhoneCommandResponse} message PhoneCommandResponse
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    PhoneCommandResponse.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.result = options.enums === String ? "PHONE_COMMAND_RESULT_UNSPECIFIED" : 0;
                            object.detail = "";
                        }
                        if (message.result != null && Object.hasOwnProperty.call(message, "result"))
                            object.result = options.enums === String ? $root.prodigy.api.v1.PhoneCommandResult[message.result] === undefined ? message.result : $root.prodigy.api.v1.PhoneCommandResult[message.result] : message.result;
                        if (message.detail != null && Object.hasOwnProperty.call(message, "detail"))
                            object.detail = message.detail;
                        return object;
                    };
    
                    /**
                     * Converts this PhoneCommandResponse to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.PhoneCommandResponse
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    PhoneCommandResponse.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for PhoneCommandResponse
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.PhoneCommandResponse
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    PhoneCommandResponse.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.PhoneCommandResponse";
                    };
    
                    return PhoneCommandResponse;
                })();
    
                v1.BtDeviceSummary = (function() {
    
                    /**
                     * Properties of a BtDeviceSummary.
                     * @memberof prodigy.api.v1
                     * @interface IBtDeviceSummary
                     * @property {boolean|null} [connected] BtDeviceSummary connected
                     * @property {string|null} [deviceName] BtDeviceSummary deviceName
                     */
    
                    /**
                     * Constructs a new BtDeviceSummary.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a BtDeviceSummary.
                     * @implements IBtDeviceSummary
                     * @constructor
                     * @param {prodigy.api.v1.IBtDeviceSummary=} [properties] Properties to set
                     */
                    function BtDeviceSummary(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * BtDeviceSummary connected.
                     * @member {boolean} connected
                     * @memberof prodigy.api.v1.BtDeviceSummary
                     * @instance
                     */
                    BtDeviceSummary.prototype.connected = false;
    
                    /**
                     * BtDeviceSummary deviceName.
                     * @member {string} deviceName
                     * @memberof prodigy.api.v1.BtDeviceSummary
                     * @instance
                     */
                    BtDeviceSummary.prototype.deviceName = "";
    
                    /**
                     * Creates a new BtDeviceSummary instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.BtDeviceSummary
                     * @static
                     * @param {prodigy.api.v1.IBtDeviceSummary=} [properties] Properties to set
                     * @returns {prodigy.api.v1.BtDeviceSummary} BtDeviceSummary instance
                     */
                    BtDeviceSummary.create = function create(properties) {
                        return new BtDeviceSummary(properties);
                    };
    
                    /**
                     * Encodes the specified BtDeviceSummary message. Does not implicitly {@link prodigy.api.v1.BtDeviceSummary.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.BtDeviceSummary
                     * @static
                     * @param {prodigy.api.v1.IBtDeviceSummary} message BtDeviceSummary message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    BtDeviceSummary.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.connected != null && Object.hasOwnProperty.call(message, "connected"))
                            writer.uint32(/* id 1, wireType 0 =*/8).bool(message.connected);
                        if (message.deviceName != null && Object.hasOwnProperty.call(message, "deviceName"))
                            writer.uint32(/* id 2, wireType 2 =*/18).string(message.deviceName);
                        return writer;
                    };
    
                    /**
                     * Decodes a BtDeviceSummary message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.BtDeviceSummary
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.BtDeviceSummary} BtDeviceSummary
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    BtDeviceSummary.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.BtDeviceSummary();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.connected = reader.bool();
                                    break;
                                }
                            case 2: {
                                    message.deviceName = reader.string();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a BtDeviceSummary message.
                     * @function verify
                     * @memberof prodigy.api.v1.BtDeviceSummary
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    BtDeviceSummary.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.connected != null && Object.hasOwnProperty.call(message, "connected"))
                            if (typeof message.connected !== "boolean")
                                return "connected: boolean expected";
                        if (message.deviceName != null && Object.hasOwnProperty.call(message, "deviceName"))
                            if (!$util.isString(message.deviceName))
                                return "deviceName: string expected";
                        return null;
                    };
    
                    /**
                     * Creates a BtDeviceSummary message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.BtDeviceSummary
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.BtDeviceSummary} BtDeviceSummary
                     */
                    BtDeviceSummary.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.BtDeviceSummary)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.BtDeviceSummary: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.BtDeviceSummary();
                        if (object.connected != null)
                            message.connected = Boolean(object.connected);
                        if (object.deviceName != null)
                            message.deviceName = String(object.deviceName);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a BtDeviceSummary message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.BtDeviceSummary
                     * @static
                     * @param {prodigy.api.v1.BtDeviceSummary} message BtDeviceSummary
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    BtDeviceSummary.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.connected = false;
                            object.deviceName = "";
                        }
                        if (message.connected != null && Object.hasOwnProperty.call(message, "connected"))
                            object.connected = message.connected;
                        if (message.deviceName != null && Object.hasOwnProperty.call(message, "deviceName"))
                            object.deviceName = message.deviceName;
                        return object;
                    };
    
                    /**
                     * Converts this BtDeviceSummary to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.BtDeviceSummary
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    BtDeviceSummary.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for BtDeviceSummary
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.BtDeviceSummary
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    BtDeviceSummary.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.BtDeviceSummary";
                    };
    
                    return BtDeviceSummary;
                })();
    
                v1.SystemStatus = (function() {
    
                    /**
                     * Properties of a SystemStatus.
                     * @memberof prodigy.api.v1
                     * @interface ISystemStatus
                     * @property {boolean|null} [nightMode] SystemStatus nightMode
                     * @property {string|null} [themeId] SystemStatus themeId
                     * @property {Object.<string,string>|null} [themeTokens] SystemStatus themeTokens
                     * @property {string|null} [appVersion] SystemStatus appVersion
                     * @property {prodigy.api.v1.IBtDeviceSummary|null} [bluetooth] SystemStatus bluetooth
                     * @property {number|null} [displayWidth] SystemStatus displayWidth
                     * @property {number|null} [displayHeight] SystemStatus displayHeight
                     */
    
                    /**
                     * Constructs a new SystemStatus.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a SystemStatus.
                     * @implements ISystemStatus
                     * @constructor
                     * @param {prodigy.api.v1.ISystemStatus=} [properties] Properties to set
                     */
                    function SystemStatus(properties) {
                        this.themeTokens = {};
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * SystemStatus nightMode.
                     * @member {boolean} nightMode
                     * @memberof prodigy.api.v1.SystemStatus
                     * @instance
                     */
                    SystemStatus.prototype.nightMode = false;
    
                    /**
                     * SystemStatus themeId.
                     * @member {string} themeId
                     * @memberof prodigy.api.v1.SystemStatus
                     * @instance
                     */
                    SystemStatus.prototype.themeId = "";
    
                    /**
                     * SystemStatus themeTokens.
                     * @member {Object.<string,string>} themeTokens
                     * @memberof prodigy.api.v1.SystemStatus
                     * @instance
                     */
                    SystemStatus.prototype.themeTokens = $util.emptyObject;
    
                    /**
                     * SystemStatus appVersion.
                     * @member {string} appVersion
                     * @memberof prodigy.api.v1.SystemStatus
                     * @instance
                     */
                    SystemStatus.prototype.appVersion = "";
    
                    /**
                     * SystemStatus bluetooth.
                     * @member {prodigy.api.v1.IBtDeviceSummary|null|undefined} bluetooth
                     * @memberof prodigy.api.v1.SystemStatus
                     * @instance
                     */
                    SystemStatus.prototype.bluetooth = null;
    
                    /**
                     * SystemStatus displayWidth.
                     * @member {number|null|undefined} displayWidth
                     * @memberof prodigy.api.v1.SystemStatus
                     * @instance
                     */
                    SystemStatus.prototype.displayWidth = null;
    
                    /**
                     * SystemStatus displayHeight.
                     * @member {number|null|undefined} displayHeight
                     * @memberof prodigy.api.v1.SystemStatus
                     * @instance
                     */
                    SystemStatus.prototype.displayHeight = null;
    
                    // OneOf field names bound to virtual getters and setters
                    var $oneOfFields;
    
                    // Virtual OneOf for proto3 optional field
                    Object.defineProperty(SystemStatus.prototype, "_displayWidth", {
                        get: $util.oneOfGetter($oneOfFields = ["displayWidth"]),
                        set: $util.oneOfSetter($oneOfFields)
                    });
    
                    // Virtual OneOf for proto3 optional field
                    Object.defineProperty(SystemStatus.prototype, "_displayHeight", {
                        get: $util.oneOfGetter($oneOfFields = ["displayHeight"]),
                        set: $util.oneOfSetter($oneOfFields)
                    });
    
                    /**
                     * Creates a new SystemStatus instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.SystemStatus
                     * @static
                     * @param {prodigy.api.v1.ISystemStatus=} [properties] Properties to set
                     * @returns {prodigy.api.v1.SystemStatus} SystemStatus instance
                     */
                    SystemStatus.create = function create(properties) {
                        return new SystemStatus(properties);
                    };
    
                    /**
                     * Encodes the specified SystemStatus message. Does not implicitly {@link prodigy.api.v1.SystemStatus.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.SystemStatus
                     * @static
                     * @param {prodigy.api.v1.ISystemStatus} message SystemStatus message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    SystemStatus.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.nightMode != null && Object.hasOwnProperty.call(message, "nightMode"))
                            writer.uint32(/* id 1, wireType 0 =*/8).bool(message.nightMode);
                        if (message.themeId != null && Object.hasOwnProperty.call(message, "themeId"))
                            writer.uint32(/* id 2, wireType 2 =*/18).string(message.themeId);
                        if (message.themeTokens != null && Object.hasOwnProperty.call(message, "themeTokens"))
                            for (var keys = Object.keys(message.themeTokens), i = 0; i < keys.length; ++i)
                                writer.uint32(/* id 3, wireType 2 =*/26).fork().uint32(/* id 1, wireType 2 =*/10).string(keys[i]).uint32(/* id 2, wireType 2 =*/18).string(message.themeTokens[keys[i]]).ldelim();
                        if (message.appVersion != null && Object.hasOwnProperty.call(message, "appVersion"))
                            writer.uint32(/* id 4, wireType 2 =*/34).string(message.appVersion);
                        if (message.bluetooth != null && Object.hasOwnProperty.call(message, "bluetooth"))
                            $root.prodigy.api.v1.BtDeviceSummary.encode(message.bluetooth, writer.uint32(/* id 5, wireType 2 =*/42).fork(), q + 1).ldelim();
                        if (message.displayWidth != null && Object.hasOwnProperty.call(message, "displayWidth"))
                            writer.uint32(/* id 6, wireType 0 =*/48).uint32(message.displayWidth);
                        if (message.displayHeight != null && Object.hasOwnProperty.call(message, "displayHeight"))
                            writer.uint32(/* id 7, wireType 0 =*/56).uint32(message.displayHeight);
                        return writer;
                    };
    
                    /**
                     * Decodes a SystemStatus message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.SystemStatus
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.SystemStatus} SystemStatus
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    SystemStatus.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.SystemStatus(), key, value;
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.nightMode = reader.bool();
                                    break;
                                }
                            case 2: {
                                    message.themeId = reader.string();
                                    break;
                                }
                            case 3: {
                                    if (message.themeTokens === $util.emptyObject)
                                        message.themeTokens = {};
                                    var end2 = reader.uint32() + reader.pos;
                                    key = "";
                                    value = "";
                                    while (reader.pos < end2) {
                                        var tag2 = reader.uint32();
                                        switch (tag2 >>> 3) {
                                        case 1:
                                            key = reader.string();
                                            break;
                                        case 2:
                                            value = reader.string();
                                            break;
                                        default:
                                            reader.skipType(tag2 & 7, long);
                                            break;
                                        }
                                    }
                                    if (key === "__proto__")
                                        $util.makeProp(message.themeTokens, key);
                                    message.themeTokens[key] = value;
                                    break;
                                }
                            case 4: {
                                    message.appVersion = reader.string();
                                    break;
                                }
                            case 5: {
                                    message.bluetooth = $root.prodigy.api.v1.BtDeviceSummary.decode(reader, reader.uint32(), undefined, long + 1);
                                    break;
                                }
                            case 6: {
                                    message.displayWidth = reader.uint32();
                                    break;
                                }
                            case 7: {
                                    message.displayHeight = reader.uint32();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a SystemStatus message.
                     * @function verify
                     * @memberof prodigy.api.v1.SystemStatus
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    SystemStatus.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        var properties = {};
                        if (message.nightMode != null && Object.hasOwnProperty.call(message, "nightMode"))
                            if (typeof message.nightMode !== "boolean")
                                return "nightMode: boolean expected";
                        if (message.themeId != null && Object.hasOwnProperty.call(message, "themeId"))
                            if (!$util.isString(message.themeId))
                                return "themeId: string expected";
                        if (message.themeTokens != null && Object.hasOwnProperty.call(message, "themeTokens")) {
                            if (!$util.isObject(message.themeTokens))
                                return "themeTokens: object expected";
                            var key = Object.keys(message.themeTokens);
                            for (var i = 0; i < key.length; ++i)
                                if (!$util.isString(message.themeTokens[key[i]]))
                                    return "themeTokens: string{k:string} expected";
                        }
                        if (message.appVersion != null && Object.hasOwnProperty.call(message, "appVersion"))
                            if (!$util.isString(message.appVersion))
                                return "appVersion: string expected";
                        if (message.bluetooth != null && Object.hasOwnProperty.call(message, "bluetooth")) {
                            var error = $root.prodigy.api.v1.BtDeviceSummary.verify(message.bluetooth, long + 1);
                            if (error)
                                return "bluetooth." + error;
                        }
                        if (message.displayWidth != null && Object.hasOwnProperty.call(message, "displayWidth")) {
                            properties._displayWidth = 1;
                            if (!$util.isInteger(message.displayWidth))
                                return "displayWidth: integer expected";
                        }
                        if (message.displayHeight != null && Object.hasOwnProperty.call(message, "displayHeight")) {
                            properties._displayHeight = 1;
                            if (!$util.isInteger(message.displayHeight))
                                return "displayHeight: integer expected";
                        }
                        return null;
                    };
    
                    /**
                     * Creates a SystemStatus message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.SystemStatus
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.SystemStatus} SystemStatus
                     */
                    SystemStatus.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.SystemStatus)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.SystemStatus: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.SystemStatus();
                        if (object.nightMode != null)
                            message.nightMode = Boolean(object.nightMode);
                        if (object.themeId != null)
                            message.themeId = String(object.themeId);
                        if (object.themeTokens) {
                            if (!$util.isObject(object.themeTokens))
                                throw TypeError(".prodigy.api.v1.SystemStatus.themeTokens: object expected");
                            message.themeTokens = {};
                            for (var keys = Object.keys(object.themeTokens), i = 0; i < keys.length; ++i) {
                                if (keys[i] === "__proto__")
                                    $util.makeProp(message.themeTokens, keys[i]);
                                message.themeTokens[keys[i]] = String(object.themeTokens[keys[i]]);
                            }
                        }
                        if (object.appVersion != null)
                            message.appVersion = String(object.appVersion);
                        if (object.bluetooth != null) {
                            if (!$util.isObject(object.bluetooth))
                                throw TypeError(".prodigy.api.v1.SystemStatus.bluetooth: object expected");
                            message.bluetooth = $root.prodigy.api.v1.BtDeviceSummary.fromObject(object.bluetooth, long + 1);
                        }
                        if (object.displayWidth != null)
                            message.displayWidth = object.displayWidth >>> 0;
                        if (object.displayHeight != null)
                            message.displayHeight = object.displayHeight >>> 0;
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a SystemStatus message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.SystemStatus
                     * @static
                     * @param {prodigy.api.v1.SystemStatus} message SystemStatus
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    SystemStatus.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.objects || options.defaults)
                            object.themeTokens = {};
                        if (options.defaults) {
                            object.nightMode = false;
                            object.themeId = "";
                            object.appVersion = "";
                            object.bluetooth = null;
                        }
                        if (message.nightMode != null && Object.hasOwnProperty.call(message, "nightMode"))
                            object.nightMode = message.nightMode;
                        if (message.themeId != null && Object.hasOwnProperty.call(message, "themeId"))
                            object.themeId = message.themeId;
                        var keys2;
                        if (message.themeTokens && (keys2 = Object.keys(message.themeTokens)).length) {
                            object.themeTokens = {};
                            for (var j = 0; j < keys2.length; ++j) {
                                if (keys2[j] === "__proto__")
                                    $util.makeProp(object.themeTokens, keys2[j]);
                                object.themeTokens[keys2[j]] = message.themeTokens[keys2[j]];
                            }
                        }
                        if (message.appVersion != null && Object.hasOwnProperty.call(message, "appVersion"))
                            object.appVersion = message.appVersion;
                        if (message.bluetooth != null && Object.hasOwnProperty.call(message, "bluetooth"))
                            object.bluetooth = $root.prodigy.api.v1.BtDeviceSummary.toObject(message.bluetooth, options, q + 1);
                        if (message.displayWidth != null && Object.hasOwnProperty.call(message, "displayWidth")) {
                            object.displayWidth = message.displayWidth;
                            if (options.oneofs)
                                object._displayWidth = "displayWidth";
                        }
                        if (message.displayHeight != null && Object.hasOwnProperty.call(message, "displayHeight")) {
                            object.displayHeight = message.displayHeight;
                            if (options.oneofs)
                                object._displayHeight = "displayHeight";
                        }
                        return object;
                    };
    
                    /**
                     * Converts this SystemStatus to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.SystemStatus
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    SystemStatus.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for SystemStatus
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.SystemStatus
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    SystemStatus.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.SystemStatus";
                    };
    
                    return SystemStatus;
                })();
    
                /**
                 * NotificationKind enum.
                 * @name prodigy.api.v1.NotificationKind
                 * @enum {number}
                 * @property {number} NOTIFICATION_KIND_UNSPECIFIED=0 NOTIFICATION_KIND_UNSPECIFIED value
                 * @property {number} NOTIFICATION_KIND_TOAST=1 NOTIFICATION_KIND_TOAST value
                 */
                v1.NotificationKind = (function() {
                    var valuesById = {}, values = Object.create(valuesById);
                    values[valuesById[0] = "NOTIFICATION_KIND_UNSPECIFIED"] = 0;
                    values[valuesById[1] = "NOTIFICATION_KIND_TOAST"] = 1;
                    return values;
                })();
    
                v1.PostNotificationRequest = (function() {
    
                    /**
                     * Properties of a PostNotificationRequest.
                     * @memberof prodigy.api.v1
                     * @interface IPostNotificationRequest
                     * @property {prodigy.api.v1.NotificationKind|null} [kind] PostNotificationRequest kind
                     * @property {string|null} [message] PostNotificationRequest message
                     * @property {number|null} [priority] PostNotificationRequest priority
                     * @property {number|null} [ttlMs] PostNotificationRequest ttlMs
                     */
    
                    /**
                     * Constructs a new PostNotificationRequest.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a PostNotificationRequest.
                     * @implements IPostNotificationRequest
                     * @constructor
                     * @param {prodigy.api.v1.IPostNotificationRequest=} [properties] Properties to set
                     */
                    function PostNotificationRequest(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * PostNotificationRequest kind.
                     * @member {prodigy.api.v1.NotificationKind} kind
                     * @memberof prodigy.api.v1.PostNotificationRequest
                     * @instance
                     */
                    PostNotificationRequest.prototype.kind = 0;
    
                    /**
                     * PostNotificationRequest message.
                     * @member {string} message
                     * @memberof prodigy.api.v1.PostNotificationRequest
                     * @instance
                     */
                    PostNotificationRequest.prototype.message = "";
    
                    /**
                     * PostNotificationRequest priority.
                     * @member {number|null|undefined} priority
                     * @memberof prodigy.api.v1.PostNotificationRequest
                     * @instance
                     */
                    PostNotificationRequest.prototype.priority = null;
    
                    /**
                     * PostNotificationRequest ttlMs.
                     * @member {number} ttlMs
                     * @memberof prodigy.api.v1.PostNotificationRequest
                     * @instance
                     */
                    PostNotificationRequest.prototype.ttlMs = 0;
    
                    // OneOf field names bound to virtual getters and setters
                    var $oneOfFields;
    
                    // Virtual OneOf for proto3 optional field
                    Object.defineProperty(PostNotificationRequest.prototype, "_priority", {
                        get: $util.oneOfGetter($oneOfFields = ["priority"]),
                        set: $util.oneOfSetter($oneOfFields)
                    });
    
                    /**
                     * Creates a new PostNotificationRequest instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.PostNotificationRequest
                     * @static
                     * @param {prodigy.api.v1.IPostNotificationRequest=} [properties] Properties to set
                     * @returns {prodigy.api.v1.PostNotificationRequest} PostNotificationRequest instance
                     */
                    PostNotificationRequest.create = function create(properties) {
                        return new PostNotificationRequest(properties);
                    };
    
                    /**
                     * Encodes the specified PostNotificationRequest message. Does not implicitly {@link prodigy.api.v1.PostNotificationRequest.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.PostNotificationRequest
                     * @static
                     * @param {prodigy.api.v1.IPostNotificationRequest} message PostNotificationRequest message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    PostNotificationRequest.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.kind != null && Object.hasOwnProperty.call(message, "kind"))
                            writer.uint32(/* id 1, wireType 0 =*/8).int32(message.kind);
                        if (message.message != null && Object.hasOwnProperty.call(message, "message"))
                            writer.uint32(/* id 2, wireType 2 =*/18).string(message.message);
                        if (message.priority != null && Object.hasOwnProperty.call(message, "priority"))
                            writer.uint32(/* id 3, wireType 0 =*/24).uint32(message.priority);
                        if (message.ttlMs != null && Object.hasOwnProperty.call(message, "ttlMs"))
                            writer.uint32(/* id 4, wireType 0 =*/32).uint32(message.ttlMs);
                        return writer;
                    };
    
                    /**
                     * Decodes a PostNotificationRequest message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.PostNotificationRequest
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.PostNotificationRequest} PostNotificationRequest
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    PostNotificationRequest.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.PostNotificationRequest();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.kind = reader.int32();
                                    break;
                                }
                            case 2: {
                                    message.message = reader.string();
                                    break;
                                }
                            case 3: {
                                    message.priority = reader.uint32();
                                    break;
                                }
                            case 4: {
                                    message.ttlMs = reader.uint32();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a PostNotificationRequest message.
                     * @function verify
                     * @memberof prodigy.api.v1.PostNotificationRequest
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    PostNotificationRequest.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        var properties = {};
                        if (message.kind != null && Object.hasOwnProperty.call(message, "kind"))
                            switch (message.kind) {
                            default:
                                return "kind: enum value expected";
                            case 0:
                            case 1:
                                break;
                            }
                        if (message.message != null && Object.hasOwnProperty.call(message, "message"))
                            if (!$util.isString(message.message))
                                return "message: string expected";
                        if (message.priority != null && Object.hasOwnProperty.call(message, "priority")) {
                            properties._priority = 1;
                            if (!$util.isInteger(message.priority))
                                return "priority: integer expected";
                        }
                        if (message.ttlMs != null && Object.hasOwnProperty.call(message, "ttlMs"))
                            if (!$util.isInteger(message.ttlMs))
                                return "ttlMs: integer expected";
                        return null;
                    };
    
                    /**
                     * Creates a PostNotificationRequest message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.PostNotificationRequest
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.PostNotificationRequest} PostNotificationRequest
                     */
                    PostNotificationRequest.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.PostNotificationRequest)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.PostNotificationRequest: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.PostNotificationRequest();
                        switch (object.kind) {
                        default:
                            if (typeof object.kind === "number") {
                                message.kind = object.kind;
                                break;
                            }
                            break;
                        case "NOTIFICATION_KIND_UNSPECIFIED":
                        case 0:
                            message.kind = 0;
                            break;
                        case "NOTIFICATION_KIND_TOAST":
                        case 1:
                            message.kind = 1;
                            break;
                        }
                        if (object.message != null)
                            message.message = String(object.message);
                        if (object.priority != null)
                            message.priority = object.priority >>> 0;
                        if (object.ttlMs != null)
                            message.ttlMs = object.ttlMs >>> 0;
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a PostNotificationRequest message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.PostNotificationRequest
                     * @static
                     * @param {prodigy.api.v1.PostNotificationRequest} message PostNotificationRequest
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    PostNotificationRequest.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.kind = options.enums === String ? "NOTIFICATION_KIND_UNSPECIFIED" : 0;
                            object.message = "";
                            object.ttlMs = 0;
                        }
                        if (message.kind != null && Object.hasOwnProperty.call(message, "kind"))
                            object.kind = options.enums === String ? $root.prodigy.api.v1.NotificationKind[message.kind] === undefined ? message.kind : $root.prodigy.api.v1.NotificationKind[message.kind] : message.kind;
                        if (message.message != null && Object.hasOwnProperty.call(message, "message"))
                            object.message = message.message;
                        if (message.priority != null && Object.hasOwnProperty.call(message, "priority")) {
                            object.priority = message.priority;
                            if (options.oneofs)
                                object._priority = "priority";
                        }
                        if (message.ttlMs != null && Object.hasOwnProperty.call(message, "ttlMs"))
                            object.ttlMs = message.ttlMs;
                        return object;
                    };
    
                    /**
                     * Converts this PostNotificationRequest to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.PostNotificationRequest
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    PostNotificationRequest.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for PostNotificationRequest
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.PostNotificationRequest
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    PostNotificationRequest.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.PostNotificationRequest";
                    };
    
                    return PostNotificationRequest;
                })();
    
                v1.PostNotificationResponse = (function() {
    
                    /**
                     * Properties of a PostNotificationResponse.
                     * @memberof prodigy.api.v1
                     * @interface IPostNotificationResponse
                     * @property {string|null} [notificationId] PostNotificationResponse notificationId
                     */
    
                    /**
                     * Constructs a new PostNotificationResponse.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a PostNotificationResponse.
                     * @implements IPostNotificationResponse
                     * @constructor
                     * @param {prodigy.api.v1.IPostNotificationResponse=} [properties] Properties to set
                     */
                    function PostNotificationResponse(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * PostNotificationResponse notificationId.
                     * @member {string} notificationId
                     * @memberof prodigy.api.v1.PostNotificationResponse
                     * @instance
                     */
                    PostNotificationResponse.prototype.notificationId = "";
    
                    /**
                     * Creates a new PostNotificationResponse instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.PostNotificationResponse
                     * @static
                     * @param {prodigy.api.v1.IPostNotificationResponse=} [properties] Properties to set
                     * @returns {prodigy.api.v1.PostNotificationResponse} PostNotificationResponse instance
                     */
                    PostNotificationResponse.create = function create(properties) {
                        return new PostNotificationResponse(properties);
                    };
    
                    /**
                     * Encodes the specified PostNotificationResponse message. Does not implicitly {@link prodigy.api.v1.PostNotificationResponse.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.PostNotificationResponse
                     * @static
                     * @param {prodigy.api.v1.IPostNotificationResponse} message PostNotificationResponse message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    PostNotificationResponse.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.notificationId != null && Object.hasOwnProperty.call(message, "notificationId"))
                            writer.uint32(/* id 1, wireType 2 =*/10).string(message.notificationId);
                        return writer;
                    };
    
                    /**
                     * Decodes a PostNotificationResponse message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.PostNotificationResponse
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.PostNotificationResponse} PostNotificationResponse
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    PostNotificationResponse.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.PostNotificationResponse();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.notificationId = reader.string();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a PostNotificationResponse message.
                     * @function verify
                     * @memberof prodigy.api.v1.PostNotificationResponse
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    PostNotificationResponse.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.notificationId != null && Object.hasOwnProperty.call(message, "notificationId"))
                            if (!$util.isString(message.notificationId))
                                return "notificationId: string expected";
                        return null;
                    };
    
                    /**
                     * Creates a PostNotificationResponse message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.PostNotificationResponse
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.PostNotificationResponse} PostNotificationResponse
                     */
                    PostNotificationResponse.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.PostNotificationResponse)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.PostNotificationResponse: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.PostNotificationResponse();
                        if (object.notificationId != null)
                            message.notificationId = String(object.notificationId);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a PostNotificationResponse message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.PostNotificationResponse
                     * @static
                     * @param {prodigy.api.v1.PostNotificationResponse} message PostNotificationResponse
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    PostNotificationResponse.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults)
                            object.notificationId = "";
                        if (message.notificationId != null && Object.hasOwnProperty.call(message, "notificationId"))
                            object.notificationId = message.notificationId;
                        return object;
                    };
    
                    /**
                     * Converts this PostNotificationResponse to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.PostNotificationResponse
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    PostNotificationResponse.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for PostNotificationResponse
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.PostNotificationResponse
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    PostNotificationResponse.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.PostNotificationResponse";
                    };
    
                    return PostNotificationResponse;
                })();
    
                v1.DismissNotificationRequest = (function() {
    
                    /**
                     * Properties of a DismissNotificationRequest.
                     * @memberof prodigy.api.v1
                     * @interface IDismissNotificationRequest
                     * @property {string|null} [notificationId] DismissNotificationRequest notificationId
                     */
    
                    /**
                     * Constructs a new DismissNotificationRequest.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a DismissNotificationRequest.
                     * @implements IDismissNotificationRequest
                     * @constructor
                     * @param {prodigy.api.v1.IDismissNotificationRequest=} [properties] Properties to set
                     */
                    function DismissNotificationRequest(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * DismissNotificationRequest notificationId.
                     * @member {string} notificationId
                     * @memberof prodigy.api.v1.DismissNotificationRequest
                     * @instance
                     */
                    DismissNotificationRequest.prototype.notificationId = "";
    
                    /**
                     * Creates a new DismissNotificationRequest instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.DismissNotificationRequest
                     * @static
                     * @param {prodigy.api.v1.IDismissNotificationRequest=} [properties] Properties to set
                     * @returns {prodigy.api.v1.DismissNotificationRequest} DismissNotificationRequest instance
                     */
                    DismissNotificationRequest.create = function create(properties) {
                        return new DismissNotificationRequest(properties);
                    };
    
                    /**
                     * Encodes the specified DismissNotificationRequest message. Does not implicitly {@link prodigy.api.v1.DismissNotificationRequest.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.DismissNotificationRequest
                     * @static
                     * @param {prodigy.api.v1.IDismissNotificationRequest} message DismissNotificationRequest message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    DismissNotificationRequest.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.notificationId != null && Object.hasOwnProperty.call(message, "notificationId"))
                            writer.uint32(/* id 1, wireType 2 =*/10).string(message.notificationId);
                        return writer;
                    };
    
                    /**
                     * Decodes a DismissNotificationRequest message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.DismissNotificationRequest
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.DismissNotificationRequest} DismissNotificationRequest
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    DismissNotificationRequest.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.DismissNotificationRequest();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.notificationId = reader.string();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a DismissNotificationRequest message.
                     * @function verify
                     * @memberof prodigy.api.v1.DismissNotificationRequest
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    DismissNotificationRequest.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.notificationId != null && Object.hasOwnProperty.call(message, "notificationId"))
                            if (!$util.isString(message.notificationId))
                                return "notificationId: string expected";
                        return null;
                    };
    
                    /**
                     * Creates a DismissNotificationRequest message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.DismissNotificationRequest
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.DismissNotificationRequest} DismissNotificationRequest
                     */
                    DismissNotificationRequest.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.DismissNotificationRequest)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.DismissNotificationRequest: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.DismissNotificationRequest();
                        if (object.notificationId != null)
                            message.notificationId = String(object.notificationId);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a DismissNotificationRequest message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.DismissNotificationRequest
                     * @static
                     * @param {prodigy.api.v1.DismissNotificationRequest} message DismissNotificationRequest
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    DismissNotificationRequest.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults)
                            object.notificationId = "";
                        if (message.notificationId != null && Object.hasOwnProperty.call(message, "notificationId"))
                            object.notificationId = message.notificationId;
                        return object;
                    };
    
                    /**
                     * Converts this DismissNotificationRequest to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.DismissNotificationRequest
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    DismissNotificationRequest.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for DismissNotificationRequest
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.DismissNotificationRequest
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    DismissNotificationRequest.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.DismissNotificationRequest";
                    };
    
                    return DismissNotificationRequest;
                })();
    
                v1.GpsReport = (function() {
    
                    /**
                     * Properties of a GpsReport.
                     * @memberof prodigy.api.v1
                     * @interface IGpsReport
                     * @property {number|null} [latitude] GpsReport latitude
                     * @property {number|null} [longitude] GpsReport longitude
                     * @property {number|null} [speedMps] GpsReport speedMps
                     * @property {number|null} [bearingDeg] GpsReport bearingDeg
                     * @property {number|null} [accuracyM] GpsReport accuracyM
                     * @property {number|null} [ageMs] GpsReport ageMs
                     * @property {number|null} [altitudeM] GpsReport altitudeM
                     */
    
                    /**
                     * Constructs a new GpsReport.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a GpsReport.
                     * @implements IGpsReport
                     * @constructor
                     * @param {prodigy.api.v1.IGpsReport=} [properties] Properties to set
                     */
                    function GpsReport(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * GpsReport latitude.
                     * @member {number} latitude
                     * @memberof prodigy.api.v1.GpsReport
                     * @instance
                     */
                    GpsReport.prototype.latitude = 0;
    
                    /**
                     * GpsReport longitude.
                     * @member {number} longitude
                     * @memberof prodigy.api.v1.GpsReport
                     * @instance
                     */
                    GpsReport.prototype.longitude = 0;
    
                    /**
                     * GpsReport speedMps.
                     * @member {number} speedMps
                     * @memberof prodigy.api.v1.GpsReport
                     * @instance
                     */
                    GpsReport.prototype.speedMps = 0;
    
                    /**
                     * GpsReport bearingDeg.
                     * @member {number} bearingDeg
                     * @memberof prodigy.api.v1.GpsReport
                     * @instance
                     */
                    GpsReport.prototype.bearingDeg = 0;
    
                    /**
                     * GpsReport accuracyM.
                     * @member {number} accuracyM
                     * @memberof prodigy.api.v1.GpsReport
                     * @instance
                     */
                    GpsReport.prototype.accuracyM = 0;
    
                    /**
                     * GpsReport ageMs.
                     * @member {number} ageMs
                     * @memberof prodigy.api.v1.GpsReport
                     * @instance
                     */
                    GpsReport.prototype.ageMs = 0;
    
                    /**
                     * GpsReport altitudeM.
                     * @member {number|null|undefined} altitudeM
                     * @memberof prodigy.api.v1.GpsReport
                     * @instance
                     */
                    GpsReport.prototype.altitudeM = null;
    
                    // OneOf field names bound to virtual getters and setters
                    var $oneOfFields;
    
                    // Virtual OneOf for proto3 optional field
                    Object.defineProperty(GpsReport.prototype, "_altitudeM", {
                        get: $util.oneOfGetter($oneOfFields = ["altitudeM"]),
                        set: $util.oneOfSetter($oneOfFields)
                    });
    
                    /**
                     * Creates a new GpsReport instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.GpsReport
                     * @static
                     * @param {prodigy.api.v1.IGpsReport=} [properties] Properties to set
                     * @returns {prodigy.api.v1.GpsReport} GpsReport instance
                     */
                    GpsReport.create = function create(properties) {
                        return new GpsReport(properties);
                    };
    
                    /**
                     * Encodes the specified GpsReport message. Does not implicitly {@link prodigy.api.v1.GpsReport.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.GpsReport
                     * @static
                     * @param {prodigy.api.v1.IGpsReport} message GpsReport message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    GpsReport.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.latitude != null && Object.hasOwnProperty.call(message, "latitude"))
                            writer.uint32(/* id 1, wireType 1 =*/9).double(message.latitude);
                        if (message.longitude != null && Object.hasOwnProperty.call(message, "longitude"))
                            writer.uint32(/* id 2, wireType 1 =*/17).double(message.longitude);
                        if (message.speedMps != null && Object.hasOwnProperty.call(message, "speedMps"))
                            writer.uint32(/* id 3, wireType 1 =*/25).double(message.speedMps);
                        if (message.bearingDeg != null && Object.hasOwnProperty.call(message, "bearingDeg"))
                            writer.uint32(/* id 4, wireType 1 =*/33).double(message.bearingDeg);
                        if (message.accuracyM != null && Object.hasOwnProperty.call(message, "accuracyM"))
                            writer.uint32(/* id 5, wireType 1 =*/41).double(message.accuracyM);
                        if (message.ageMs != null && Object.hasOwnProperty.call(message, "ageMs"))
                            writer.uint32(/* id 6, wireType 0 =*/48).uint32(message.ageMs);
                        if (message.altitudeM != null && Object.hasOwnProperty.call(message, "altitudeM"))
                            writer.uint32(/* id 7, wireType 1 =*/57).double(message.altitudeM);
                        return writer;
                    };
    
                    /**
                     * Decodes a GpsReport message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.GpsReport
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.GpsReport} GpsReport
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    GpsReport.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.GpsReport();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.latitude = reader.double();
                                    break;
                                }
                            case 2: {
                                    message.longitude = reader.double();
                                    break;
                                }
                            case 3: {
                                    message.speedMps = reader.double();
                                    break;
                                }
                            case 4: {
                                    message.bearingDeg = reader.double();
                                    break;
                                }
                            case 5: {
                                    message.accuracyM = reader.double();
                                    break;
                                }
                            case 6: {
                                    message.ageMs = reader.uint32();
                                    break;
                                }
                            case 7: {
                                    message.altitudeM = reader.double();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a GpsReport message.
                     * @function verify
                     * @memberof prodigy.api.v1.GpsReport
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    GpsReport.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        var properties = {};
                        if (message.latitude != null && Object.hasOwnProperty.call(message, "latitude"))
                            if (typeof message.latitude !== "number")
                                return "latitude: number expected";
                        if (message.longitude != null && Object.hasOwnProperty.call(message, "longitude"))
                            if (typeof message.longitude !== "number")
                                return "longitude: number expected";
                        if (message.speedMps != null && Object.hasOwnProperty.call(message, "speedMps"))
                            if (typeof message.speedMps !== "number")
                                return "speedMps: number expected";
                        if (message.bearingDeg != null && Object.hasOwnProperty.call(message, "bearingDeg"))
                            if (typeof message.bearingDeg !== "number")
                                return "bearingDeg: number expected";
                        if (message.accuracyM != null && Object.hasOwnProperty.call(message, "accuracyM"))
                            if (typeof message.accuracyM !== "number")
                                return "accuracyM: number expected";
                        if (message.ageMs != null && Object.hasOwnProperty.call(message, "ageMs"))
                            if (!$util.isInteger(message.ageMs))
                                return "ageMs: integer expected";
                        if (message.altitudeM != null && Object.hasOwnProperty.call(message, "altitudeM")) {
                            properties._altitudeM = 1;
                            if (typeof message.altitudeM !== "number")
                                return "altitudeM: number expected";
                        }
                        return null;
                    };
    
                    /**
                     * Creates a GpsReport message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.GpsReport
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.GpsReport} GpsReport
                     */
                    GpsReport.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.GpsReport)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.GpsReport: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.GpsReport();
                        if (object.latitude != null)
                            message.latitude = Number(object.latitude);
                        if (object.longitude != null)
                            message.longitude = Number(object.longitude);
                        if (object.speedMps != null)
                            message.speedMps = Number(object.speedMps);
                        if (object.bearingDeg != null)
                            message.bearingDeg = Number(object.bearingDeg);
                        if (object.accuracyM != null)
                            message.accuracyM = Number(object.accuracyM);
                        if (object.ageMs != null)
                            message.ageMs = object.ageMs >>> 0;
                        if (object.altitudeM != null)
                            message.altitudeM = Number(object.altitudeM);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a GpsReport message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.GpsReport
                     * @static
                     * @param {prodigy.api.v1.GpsReport} message GpsReport
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    GpsReport.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.latitude = 0;
                            object.longitude = 0;
                            object.speedMps = 0;
                            object.bearingDeg = 0;
                            object.accuracyM = 0;
                            object.ageMs = 0;
                        }
                        if (message.latitude != null && Object.hasOwnProperty.call(message, "latitude"))
                            object.latitude = options.json && !isFinite(message.latitude) ? String(message.latitude) : message.latitude;
                        if (message.longitude != null && Object.hasOwnProperty.call(message, "longitude"))
                            object.longitude = options.json && !isFinite(message.longitude) ? String(message.longitude) : message.longitude;
                        if (message.speedMps != null && Object.hasOwnProperty.call(message, "speedMps"))
                            object.speedMps = options.json && !isFinite(message.speedMps) ? String(message.speedMps) : message.speedMps;
                        if (message.bearingDeg != null && Object.hasOwnProperty.call(message, "bearingDeg"))
                            object.bearingDeg = options.json && !isFinite(message.bearingDeg) ? String(message.bearingDeg) : message.bearingDeg;
                        if (message.accuracyM != null && Object.hasOwnProperty.call(message, "accuracyM"))
                            object.accuracyM = options.json && !isFinite(message.accuracyM) ? String(message.accuracyM) : message.accuracyM;
                        if (message.ageMs != null && Object.hasOwnProperty.call(message, "ageMs"))
                            object.ageMs = message.ageMs;
                        if (message.altitudeM != null && Object.hasOwnProperty.call(message, "altitudeM")) {
                            object.altitudeM = options.json && !isFinite(message.altitudeM) ? String(message.altitudeM) : message.altitudeM;
                            if (options.oneofs)
                                object._altitudeM = "altitudeM";
                        }
                        return object;
                    };
    
                    /**
                     * Converts this GpsReport to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.GpsReport
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    GpsReport.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for GpsReport
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.GpsReport
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    GpsReport.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.GpsReport";
                    };
    
                    return GpsReport;
                })();
    
                v1.BatteryReport = (function() {
    
                    /**
                     * Properties of a BatteryReport.
                     * @memberof prodigy.api.v1
                     * @interface IBatteryReport
                     * @property {number|null} [percent] BatteryReport percent
                     * @property {boolean|null} [charging] BatteryReport charging
                     */
    
                    /**
                     * Constructs a new BatteryReport.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a BatteryReport.
                     * @implements IBatteryReport
                     * @constructor
                     * @param {prodigy.api.v1.IBatteryReport=} [properties] Properties to set
                     */
                    function BatteryReport(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * BatteryReport percent.
                     * @member {number} percent
                     * @memberof prodigy.api.v1.BatteryReport
                     * @instance
                     */
                    BatteryReport.prototype.percent = 0;
    
                    /**
                     * BatteryReport charging.
                     * @member {boolean} charging
                     * @memberof prodigy.api.v1.BatteryReport
                     * @instance
                     */
                    BatteryReport.prototype.charging = false;
    
                    /**
                     * Creates a new BatteryReport instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.BatteryReport
                     * @static
                     * @param {prodigy.api.v1.IBatteryReport=} [properties] Properties to set
                     * @returns {prodigy.api.v1.BatteryReport} BatteryReport instance
                     */
                    BatteryReport.create = function create(properties) {
                        return new BatteryReport(properties);
                    };
    
                    /**
                     * Encodes the specified BatteryReport message. Does not implicitly {@link prodigy.api.v1.BatteryReport.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.BatteryReport
                     * @static
                     * @param {prodigy.api.v1.IBatteryReport} message BatteryReport message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    BatteryReport.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.percent != null && Object.hasOwnProperty.call(message, "percent"))
                            writer.uint32(/* id 1, wireType 0 =*/8).uint32(message.percent);
                        if (message.charging != null && Object.hasOwnProperty.call(message, "charging"))
                            writer.uint32(/* id 2, wireType 0 =*/16).bool(message.charging);
                        return writer;
                    };
    
                    /**
                     * Decodes a BatteryReport message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.BatteryReport
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.BatteryReport} BatteryReport
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    BatteryReport.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.BatteryReport();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.percent = reader.uint32();
                                    break;
                                }
                            case 2: {
                                    message.charging = reader.bool();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a BatteryReport message.
                     * @function verify
                     * @memberof prodigy.api.v1.BatteryReport
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    BatteryReport.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        if (message.percent != null && Object.hasOwnProperty.call(message, "percent"))
                            if (!$util.isInteger(message.percent))
                                return "percent: integer expected";
                        if (message.charging != null && Object.hasOwnProperty.call(message, "charging"))
                            if (typeof message.charging !== "boolean")
                                return "charging: boolean expected";
                        return null;
                    };
    
                    /**
                     * Creates a BatteryReport message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.BatteryReport
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.BatteryReport} BatteryReport
                     */
                    BatteryReport.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.BatteryReport)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.BatteryReport: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.BatteryReport();
                        if (object.percent != null)
                            message.percent = object.percent >>> 0;
                        if (object.charging != null)
                            message.charging = Boolean(object.charging);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a BatteryReport message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.BatteryReport
                     * @static
                     * @param {prodigy.api.v1.BatteryReport} message BatteryReport
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    BatteryReport.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.percent = 0;
                            object.charging = false;
                        }
                        if (message.percent != null && Object.hasOwnProperty.call(message, "percent"))
                            object.percent = message.percent;
                        if (message.charging != null && Object.hasOwnProperty.call(message, "charging"))
                            object.charging = message.charging;
                        return object;
                    };
    
                    /**
                     * Converts this BatteryReport to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.BatteryReport
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    BatteryReport.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for BatteryReport
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.BatteryReport
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    BatteryReport.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.BatteryReport";
                    };
    
                    return BatteryReport;
                })();
    
                v1.ConnectivityReport = (function() {
    
                    /**
                     * Properties of a ConnectivityReport.
                     * @memberof prodigy.api.v1
                     * @interface IConnectivityReport
                     * @property {boolean|null} [internetAvailable] ConnectivityReport internetAvailable
                     * @property {boolean|null} [socks5Active] ConnectivityReport socks5Active
                     * @property {number|null} [socks5Port] ConnectivityReport socks5Port
                     * @property {string|null} [socks5Password] ConnectivityReport socks5Password
                     */
    
                    /**
                     * Constructs a new ConnectivityReport.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a ConnectivityReport.
                     * @implements IConnectivityReport
                     * @constructor
                     * @param {prodigy.api.v1.IConnectivityReport=} [properties] Properties to set
                     */
                    function ConnectivityReport(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * ConnectivityReport internetAvailable.
                     * @member {boolean} internetAvailable
                     * @memberof prodigy.api.v1.ConnectivityReport
                     * @instance
                     */
                    ConnectivityReport.prototype.internetAvailable = false;
    
                    /**
                     * ConnectivityReport socks5Active.
                     * @member {boolean} socks5Active
                     * @memberof prodigy.api.v1.ConnectivityReport
                     * @instance
                     */
                    ConnectivityReport.prototype.socks5Active = false;
    
                    /**
                     * ConnectivityReport socks5Port.
                     * @member {number} socks5Port
                     * @memberof prodigy.api.v1.ConnectivityReport
                     * @instance
                     */
                    ConnectivityReport.prototype.socks5Port = 0;
    
                    /**
                     * ConnectivityReport socks5Password.
                     * @member {string|null|undefined} socks5Password
                     * @memberof prodigy.api.v1.ConnectivityReport
                     * @instance
                     */
                    ConnectivityReport.prototype.socks5Password = null;
    
                    // OneOf field names bound to virtual getters and setters
                    var $oneOfFields;
    
                    // Virtual OneOf for proto3 optional field
                    Object.defineProperty(ConnectivityReport.prototype, "_socks5Password", {
                        get: $util.oneOfGetter($oneOfFields = ["socks5Password"]),
                        set: $util.oneOfSetter($oneOfFields)
                    });
    
                    /**
                     * Creates a new ConnectivityReport instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.ConnectivityReport
                     * @static
                     * @param {prodigy.api.v1.IConnectivityReport=} [properties] Properties to set
                     * @returns {prodigy.api.v1.ConnectivityReport} ConnectivityReport instance
                     */
                    ConnectivityReport.create = function create(properties) {
                        return new ConnectivityReport(properties);
                    };
    
                    /**
                     * Encodes the specified ConnectivityReport message. Does not implicitly {@link prodigy.api.v1.ConnectivityReport.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.ConnectivityReport
                     * @static
                     * @param {prodigy.api.v1.IConnectivityReport} message ConnectivityReport message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    ConnectivityReport.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.internetAvailable != null && Object.hasOwnProperty.call(message, "internetAvailable"))
                            writer.uint32(/* id 1, wireType 0 =*/8).bool(message.internetAvailable);
                        if (message.socks5Active != null && Object.hasOwnProperty.call(message, "socks5Active"))
                            writer.uint32(/* id 2, wireType 0 =*/16).bool(message.socks5Active);
                        if (message.socks5Port != null && Object.hasOwnProperty.call(message, "socks5Port"))
                            writer.uint32(/* id 3, wireType 0 =*/24).uint32(message.socks5Port);
                        if (message.socks5Password != null && Object.hasOwnProperty.call(message, "socks5Password"))
                            writer.uint32(/* id 4, wireType 2 =*/34).string(message.socks5Password);
                        return writer;
                    };
    
                    /**
                     * Decodes a ConnectivityReport message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.ConnectivityReport
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.ConnectivityReport} ConnectivityReport
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    ConnectivityReport.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.ConnectivityReport();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.internetAvailable = reader.bool();
                                    break;
                                }
                            case 2: {
                                    message.socks5Active = reader.bool();
                                    break;
                                }
                            case 3: {
                                    message.socks5Port = reader.uint32();
                                    break;
                                }
                            case 4: {
                                    message.socks5Password = reader.string();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a ConnectivityReport message.
                     * @function verify
                     * @memberof prodigy.api.v1.ConnectivityReport
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    ConnectivityReport.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        var properties = {};
                        if (message.internetAvailable != null && Object.hasOwnProperty.call(message, "internetAvailable"))
                            if (typeof message.internetAvailable !== "boolean")
                                return "internetAvailable: boolean expected";
                        if (message.socks5Active != null && Object.hasOwnProperty.call(message, "socks5Active"))
                            if (typeof message.socks5Active !== "boolean")
                                return "socks5Active: boolean expected";
                        if (message.socks5Port != null && Object.hasOwnProperty.call(message, "socks5Port"))
                            if (!$util.isInteger(message.socks5Port))
                                return "socks5Port: integer expected";
                        if (message.socks5Password != null && Object.hasOwnProperty.call(message, "socks5Password")) {
                            properties._socks5Password = 1;
                            if (!$util.isString(message.socks5Password))
                                return "socks5Password: string expected";
                        }
                        return null;
                    };
    
                    /**
                     * Creates a ConnectivityReport message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.ConnectivityReport
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.ConnectivityReport} ConnectivityReport
                     */
                    ConnectivityReport.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.ConnectivityReport)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.ConnectivityReport: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.ConnectivityReport();
                        if (object.internetAvailable != null)
                            message.internetAvailable = Boolean(object.internetAvailable);
                        if (object.socks5Active != null)
                            message.socks5Active = Boolean(object.socks5Active);
                        if (object.socks5Port != null)
                            message.socks5Port = object.socks5Port >>> 0;
                        if (object.socks5Password != null)
                            message.socks5Password = String(object.socks5Password);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a ConnectivityReport message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.ConnectivityReport
                     * @static
                     * @param {prodigy.api.v1.ConnectivityReport} message ConnectivityReport
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    ConnectivityReport.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults) {
                            object.internetAvailable = false;
                            object.socks5Active = false;
                            object.socks5Port = 0;
                        }
                        if (message.internetAvailable != null && Object.hasOwnProperty.call(message, "internetAvailable"))
                            object.internetAvailable = message.internetAvailable;
                        if (message.socks5Active != null && Object.hasOwnProperty.call(message, "socks5Active"))
                            object.socks5Active = message.socks5Active;
                        if (message.socks5Port != null && Object.hasOwnProperty.call(message, "socks5Port"))
                            object.socks5Port = message.socks5Port;
                        if (message.socks5Password != null && Object.hasOwnProperty.call(message, "socks5Password")) {
                            object.socks5Password = message.socks5Password;
                            if (options.oneofs)
                                object._socks5Password = "socks5Password";
                        }
                        return object;
                    };
    
                    /**
                     * Converts this ConnectivityReport to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.ConnectivityReport
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    ConnectivityReport.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for ConnectivityReport
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.ConnectivityReport
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    ConnectivityReport.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.ConnectivityReport";
                    };
    
                    return ConnectivityReport;
                })();
    
                v1.TimeReport = (function() {
    
                    /**
                     * Properties of a TimeReport.
                     * @memberof prodigy.api.v1
                     * @interface ITimeReport
                     * @property {number|Long|null} [unixTimeMs] TimeReport unixTimeMs
                     * @property {string|null} [timezoneId] TimeReport timezoneId
                     */
    
                    /**
                     * Constructs a new TimeReport.
                     * @memberof prodigy.api.v1
                     * @classdesc Represents a TimeReport.
                     * @implements ITimeReport
                     * @constructor
                     * @param {prodigy.api.v1.ITimeReport=} [properties] Properties to set
                     */
                    function TimeReport(properties) {
                        if (properties)
                            for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                                if (properties[keys[i]] != null && keys[i] !== "__proto__")
                                    this[keys[i]] = properties[keys[i]];
                    }
    
                    /**
                     * TimeReport unixTimeMs.
                     * @member {number|Long} unixTimeMs
                     * @memberof prodigy.api.v1.TimeReport
                     * @instance
                     */
                    TimeReport.prototype.unixTimeMs = $util.Long ? $util.Long.fromBits(0,0,false) : 0;
    
                    /**
                     * TimeReport timezoneId.
                     * @member {string|null|undefined} timezoneId
                     * @memberof prodigy.api.v1.TimeReport
                     * @instance
                     */
                    TimeReport.prototype.timezoneId = null;
    
                    // OneOf field names bound to virtual getters and setters
                    var $oneOfFields;
    
                    // Virtual OneOf for proto3 optional field
                    Object.defineProperty(TimeReport.prototype, "_timezoneId", {
                        get: $util.oneOfGetter($oneOfFields = ["timezoneId"]),
                        set: $util.oneOfSetter($oneOfFields)
                    });
    
                    /**
                     * Creates a new TimeReport instance using the specified properties.
                     * @function create
                     * @memberof prodigy.api.v1.TimeReport
                     * @static
                     * @param {prodigy.api.v1.ITimeReport=} [properties] Properties to set
                     * @returns {prodigy.api.v1.TimeReport} TimeReport instance
                     */
                    TimeReport.create = function create(properties) {
                        return new TimeReport(properties);
                    };
    
                    /**
                     * Encodes the specified TimeReport message. Does not implicitly {@link prodigy.api.v1.TimeReport.verify|verify} messages.
                     * @function encode
                     * @memberof prodigy.api.v1.TimeReport
                     * @static
                     * @param {prodigy.api.v1.ITimeReport} message TimeReport message or plain object to encode
                     * @param {$protobuf.Writer} [writer] Writer to encode to
                     * @returns {$protobuf.Writer} Writer
                     */
                    TimeReport.encode = function encode(message, writer, q) {
                        if (!writer)
                            writer = $Writer.create();
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        if (message.unixTimeMs != null && Object.hasOwnProperty.call(message, "unixTimeMs"))
                            writer.uint32(/* id 1, wireType 0 =*/8).int64(message.unixTimeMs);
                        if (message.timezoneId != null && Object.hasOwnProperty.call(message, "timezoneId"))
                            writer.uint32(/* id 2, wireType 2 =*/18).string(message.timezoneId);
                        return writer;
                    };
    
                    /**
                     * Decodes a TimeReport message from the specified reader or buffer.
                     * @function decode
                     * @memberof prodigy.api.v1.TimeReport
                     * @static
                     * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
                     * @param {number} [length] Message length if known beforehand
                     * @returns {prodigy.api.v1.TimeReport} TimeReport
                     * @throws {Error} If the payload is not a reader or valid buffer
                     * @throws {$protobuf.util.ProtocolError} If required fields are missing
                     */
                    TimeReport.decode = function decode(reader, length, error, long) {
                        if (!(reader instanceof $Reader))
                            reader = $Reader.create(reader);
                        if (long === undefined)
                            long = 0;
                        if (long > $Reader.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var end = length === undefined ? reader.len : reader.pos + length, message = new $root.prodigy.api.v1.TimeReport();
                        while (reader.pos < end) {
                            var tag = reader.uint32();
                            if (tag === error)
                                break;
                            switch (tag >>> 3) {
                            case 1: {
                                    message.unixTimeMs = reader.int64();
                                    break;
                                }
                            case 2: {
                                    message.timezoneId = reader.string();
                                    break;
                                }
                            default:
                                reader.skipType(tag & 7, long);
                                break;
                            }
                        }
                        return message;
                    };
    
                    /**
                     * Verifies a TimeReport message.
                     * @function verify
                     * @memberof prodigy.api.v1.TimeReport
                     * @static
                     * @param {Object.<string,*>} message Plain object to verify
                     * @returns {string|null} `null` if valid, otherwise the reason why it is not
                     */
                    TimeReport.verify = function verify(message, long) {
                        if (typeof message !== "object" || message === null)
                            return "object expected";
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            return "maximum nesting depth exceeded";
                        var properties = {};
                        if (message.unixTimeMs != null && Object.hasOwnProperty.call(message, "unixTimeMs"))
                            if (!$util.isInteger(message.unixTimeMs) && !(message.unixTimeMs && $util.isInteger(message.unixTimeMs.low) && $util.isInteger(message.unixTimeMs.high)))
                                return "unixTimeMs: integer|Long expected";
                        if (message.timezoneId != null && Object.hasOwnProperty.call(message, "timezoneId")) {
                            properties._timezoneId = 1;
                            if (!$util.isString(message.timezoneId))
                                return "timezoneId: string expected";
                        }
                        return null;
                    };
    
                    /**
                     * Creates a TimeReport message from a plain object. Also converts values to their respective internal types.
                     * @function fromObject
                     * @memberof prodigy.api.v1.TimeReport
                     * @static
                     * @param {Object.<string,*>} object Plain object
                     * @returns {prodigy.api.v1.TimeReport} TimeReport
                     */
                    TimeReport.fromObject = function fromObject(object, long) {
                        if (object instanceof $root.prodigy.api.v1.TimeReport)
                            return object;
                        if (!$util.isObject(object))
                            throw TypeError(".prodigy.api.v1.TimeReport: object expected");
                        if (long === undefined)
                            long = 0;
                        if (long > $util.recursionLimit)
                            throw Error("maximum nesting depth exceeded");
                        var message = new $root.prodigy.api.v1.TimeReport();
                        if (object.unixTimeMs != null)
                            if ($util.Long)
                                message.unixTimeMs = $util.Long.fromValue(object.unixTimeMs, false);
                            else if (typeof object.unixTimeMs === "string")
                                message.unixTimeMs = parseInt(object.unixTimeMs, 10);
                            else if (typeof object.unixTimeMs === "number")
                                message.unixTimeMs = object.unixTimeMs;
                            else if (typeof object.unixTimeMs === "object")
                                message.unixTimeMs = new $util.LongBits(object.unixTimeMs.low >>> 0, object.unixTimeMs.high >>> 0).toNumber();
                        if (object.timezoneId != null)
                            message.timezoneId = String(object.timezoneId);
                        return message;
                    };
    
                    /**
                     * Creates a plain object from a TimeReport message. Also converts values to other types if specified.
                     * @function toObject
                     * @memberof prodigy.api.v1.TimeReport
                     * @static
                     * @param {prodigy.api.v1.TimeReport} message TimeReport
                     * @param {$protobuf.IConversionOptions} [options] Conversion options
                     * @returns {Object.<string,*>} Plain object
                     */
                    TimeReport.toObject = function toObject(message, options, q) {
                        if (!options)
                            options = {};
                        if (q === undefined)
                            q = 0;
                        if (q > $util.recursionLimit)
                            throw Error("max depth exceeded");
                        var object = {};
                        if (options.defaults)
                            if ($util.Long) {
                                var long = new $util.Long(0, 0, false);
                                object.unixTimeMs = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : typeof BigInt !== "undefined" && options.longs === BigInt ? long.toBigInt() : long;
                            } else
                                object.unixTimeMs = options.longs === String ? "0" : typeof BigInt !== "undefined" && options.longs === BigInt ? BigInt("0") : 0;
                        if (message.unixTimeMs != null && Object.hasOwnProperty.call(message, "unixTimeMs"))
                            if (typeof BigInt !== "undefined" && options.longs === BigInt)
                                object.unixTimeMs = typeof message.unixTimeMs === "number" ? BigInt(message.unixTimeMs) : $util.Long.fromBits(message.unixTimeMs.low >>> 0, message.unixTimeMs.high >>> 0, false).toBigInt();
                            else if (typeof message.unixTimeMs === "number")
                                object.unixTimeMs = options.longs === String ? String(message.unixTimeMs) : message.unixTimeMs;
                            else
                                object.unixTimeMs = options.longs === String ? $util.Long.prototype.toString.call(message.unixTimeMs) : options.longs === Number ? new $util.LongBits(message.unixTimeMs.low >>> 0, message.unixTimeMs.high >>> 0).toNumber() : message.unixTimeMs;
                        if (message.timezoneId != null && Object.hasOwnProperty.call(message, "timezoneId")) {
                            object.timezoneId = message.timezoneId;
                            if (options.oneofs)
                                object._timezoneId = "timezoneId";
                        }
                        return object;
                    };
    
                    /**
                     * Converts this TimeReport to JSON.
                     * @function toJSON
                     * @memberof prodigy.api.v1.TimeReport
                     * @instance
                     * @returns {Object.<string,*>} JSON object
                     */
                    TimeReport.prototype.toJSON = function toJSON() {
                        return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
                    };
    
                    /**
                     * Gets the default type url for TimeReport
                     * @function getTypeUrl
                     * @memberof prodigy.api.v1.TimeReport
                     * @static
                     * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
                     * @returns {string} The default type url
                     */
                    TimeReport.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
                        if (typeUrlPrefix === undefined) {
                            typeUrlPrefix = "type.googleapis.com";
                        }
                        return typeUrlPrefix + "/prodigy.api.v1.TimeReport";
                    };
    
                    return TimeReport;
                })();
    
                return v1;
            })();
    
            return api;
        })();
    
        return prodigy;
    })();

    return $root;
})(protobuf);
