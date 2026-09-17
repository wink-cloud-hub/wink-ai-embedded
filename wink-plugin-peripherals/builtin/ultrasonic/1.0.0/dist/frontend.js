import { definePeripheral as e, pinsFromBinderVariant as t } from "@wink-ai/unisim-ui";
import { resolvePluginIdentity as n } from "@wink-ai/unisim-sdk";
import { createElementBlock as r, createElementVNode as i, defineComponent as a, openBlock as o } from "vue";
import "@wokwi/elements";
//#endregion
//#region builtin/ultrasonic/1.0.0/src/CanvasGlyph.vue
var s = /* @__PURE__ */ a({
	__name: "CanvasGlyph",
	setup(e) {
		return (e, t) => (o(), r("wokwi-hc-sr04"));
	}
}), c = { class: "ultrasonic-widget" }, l = /*#__PURE__*/ ((e, t) => {
	let n = e.__vccOpts || e;
	for (let [e, r] of t) n[e] = r;
	return n;
})(/* @__PURE__ */ a({
	__name: "WorldWidget",
	props: {
		pluginInstanceId: {},
		pinConnections: {}
	},
	setup(e) {
		return (e, t) => (o(), r("div", c, [...t[0] ||= [i("wokwi-hc-sr04", null, null, -1)]]));
	}
}), [["__scopeId", "data-v-20a560f2"]]), u = Object.freeze({
	width: 171,
	height: 95
}), d = Object.freeze({
	VCC: Object.freeze({
		relX: 71,
		relY: 95,
		wireNet: "vcc",
		defaultConnection: "VCC",
		required: !1
	}),
	TRIG: Object.freeze({
		relX: 81,
		relY: 95,
		wireNet: "primary",
		defaultConnection: null,
		required: !0
	}),
	ECHO: Object.freeze({
		relX: 91,
		relY: 95,
		wireNet: "secondary",
		defaultConnection: null,
		required: !1
	}),
	GND: Object.freeze({
		relX: 101,
		relY: 95,
		wireNet: "gnd",
		defaultConnection: "GND",
		required: !1
	})
}), f = u, p = d, m = Object.freeze({
	SIG: Object.freeze({
		relX: 87,
		relY: 95,
		wireNet: "primary",
		defaultConnection: 12
	}),
	VCC: Object.freeze({
		relX: 72,
		relY: 95,
		wireNet: "vcc",
		defaultConnection: "VCC"
	}),
	GND: Object.freeze({
		relX: 102,
		relY: 95,
		wireNet: "gnd",
		defaultConnection: "GND"
	})
}), h = Object.freeze({
	TX: Object.freeze({
		relX: 82,
		relY: 95,
		wireNet: "secondary",
		defaultConnection: 17
	}),
	RX: Object.freeze({
		relX: 92,
		relY: 95,
		wireNet: "primary",
		defaultConnection: 16
	}),
	VCC: Object.freeze({
		relX: 72,
		relY: 95,
		wireNet: "vcc",
		defaultConnection: "VCC"
	}),
	GND: Object.freeze({
		relX: 102,
		relY: 95,
		wireNet: "gnd",
		defaultConnection: "GND"
	})
}), g = Object.freeze({
	SDA: Object.freeze({
		relX: 82,
		relY: 95,
		wireNet: "primary",
		defaultConnection: 21
	}),
	SCL: Object.freeze({
		relX: 92,
		relY: 95,
		wireNet: "secondary",
		defaultConnection: 22
	}),
	VCC: Object.freeze({
		relX: 72,
		relY: 95,
		wireNet: "vcc",
		defaultConnection: "VCC"
	}),
	GND: Object.freeze({
		relX: 102,
		relY: 95,
		wireNet: "gnd",
		defaultConnection: "GND"
	})
}), _ = p;
Object.freeze({
	hcsr04: Object.freeze({
		variant: "hcsr04",
		getPins: () => t("ultrasonic", "hcsr04"),
		pinsOverlay: p,
		defaultAppearanceId: "ultrasonic_hcsr04"
	}),
	single_pin_ping: Object.freeze({
		variant: "single_pin_ping",
		getPins: () => t("ultrasonic", "single_pin_ping"),
		pinsOverlay: m,
		defaultAppearanceId: "ultrasonic_ping"
	}),
	uart_stream: Object.freeze({
		variant: "uart_stream",
		getPins: () => t("ultrasonic", "uart_stream"),
		pinsOverlay: h,
		defaultAppearanceId: "ultrasonic_uart"
	}),
	i2c: Object.freeze({
		variant: "i2c",
		getPins: () => t("ultrasonic", "i2c"),
		pinsOverlay: g,
		defaultAppearanceId: "ultrasonic_i2c"
	})
}), Object.freeze({
	ultrasonic_hcsr04: Object.freeze({
		appearanceId: "ultrasonic_hcsr04",
		variant: "hcsr04",
		displayName: "HC-SR04 Ultrasonic Sensor (4-Pin)",
		elementTag: "wokwi-hc-sr04"
	}),
	ultrasonic_ping: Object.freeze({
		appearanceId: "ultrasonic_ping",
		variant: "single_pin_ping",
		displayName: "Parallax PING))) Ultrasonic (3-Pin)",
		elementTag: "wink-custom-ping"
	}),
	ultrasonic_uart: Object.freeze({
		appearanceId: "ultrasonic_uart",
		variant: "uart_stream",
		displayName: "US-100 UART Ultrasonic Sensor",
		elementTag: "wink-custom-ultrasonic-uart"
	}),
	ultrasonic_i2c: Object.freeze({
		appearanceId: "ultrasonic_i2c",
		variant: "i2c",
		displayName: "Devantech SRF02/SRF08 I2C Ultrasonic Sensor",
		elementTag: "wink-custom-ultrasonic-i2c"
	})
});
//#endregion
//#region builtin/ultrasonic/1.0.0/src/definition.ts
var v = n(import.meta.url, "ultrasonic", "1.0.0", "sensor"), y = e({
	type: v.type,
	catalog: {
		id: v.type,
		worldCoupling: "required"
	},
	size: f,
	wireColor: "#eab308",
	pinsOverlay: _,
	props: {
		variant: {
			type: "string",
			default: "default",
			description: "Sensor variant"
		},
		maxDistanceCm: {
			type: "number",
			default: 400,
			description: "Max Distance (cm)",
			range: {
				min: 2,
				max: 400,
				step: 1
			}
		},
		minDistanceCm: {
			type: "number",
			default: 2,
			description: "Min Distance (cm)",
			range: {
				min: 0,
				max: 10,
				step: 1
			}
		}
	},
	canvas: s,
	world: l,
	ui: { worldProps: (e) => ({
		pinConnections: e.pinConnections,
		...e.props.pluginInstanceId ? { pluginInstanceId: e.props.pluginInstanceId } : {}
	}) }
});
//#endregion
export { y as default, y as ultrasonicDefinition };
