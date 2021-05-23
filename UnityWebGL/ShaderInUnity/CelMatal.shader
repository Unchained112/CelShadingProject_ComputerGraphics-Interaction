Shader "Custom/CelMatal"
{
	Properties {
		_OutlineWidth ("Outline Width", Range(0, 0.1)) = 0.01
		_Color ("Base Color", Color) = (0.8,0.4,0.15,1)
		_Ramp ("Toon Ramp (RGB)", 2D) = "white" {} 
		[Header(Metal)]
		_Brightness("Specular Brightness", Range(0, 2)) = 1.3  
		_Offset("Specular Size", Range(0, 1)) = 0.8 //  
		_SpecuColor("Specular Color", Color) = (0.8,0.45,0.2,1)
		[Header(Highlight)]
		_HighlightOffset("Highlight Size", Range(0, 1)) = 0.9  
		_HiColor("Highlight Color", Color) = (1,1,1,1)
		[Header(Rim)]
		_RimColor("Rim Color", Color) = (1,0.3,0.3,1)
		_RimPower("Rim Power", Range(0, 20)) = 6 
			}

	SubShader {
		Tags { "RenderType"="Opaque" }
		LOD 200
		
		Pass
		{	
			Cull front

			Stencil 
			{
				Ref 1
				Comp Greater
			}

			CGPROGRAM
			#pragma vertex vert
			#pragma fragment frag

			#include "UnityCG.cginc"

			struct appdata
			{
				float4 vertex : POSITION;
				float3 normal : NORMAL;
				half4 color : COLOR;
			};

			struct v2f
			{
				float4 vertex : SV_POSITION;
			};

			half _OutlineWidth;
			static const half4 OUTLINE_COLOR = half4(0,0,0,0);

			v2f vert (appdata v)
			{
				float4 clipPosition = UnityObjectToClipPos(v.vertex);
				float3 clipNormal = mul((float3x3) UNITY_MATRIX_VP, mul((float3x3) UNITY_MATRIX_M, v.normal));
				float2 offset = normalize(clipNormal.xy) * _OutlineWidth * clipPosition.w * v.color.b;
				float aspect = _ScreenParams.x / _ScreenParams.y;
				offset.y *= aspect;
				clipPosition.xy += offset;
				v2f o;
				o.vertex = clipPosition;
				return o;
			}

			fixed4 frag () : SV_Target
			{
				return OUTLINE_COLOR;
			}
			ENDCG
		}


		
		CGPROGRAM
		#pragma surface surf ToonRamp vertex:vert

		sampler2D _Ramp;

		// custom lighting function that uses a texture ramp based
		// on angle between light direction and normal
		#pragma lighting ToonRamp exclude_path:prepass
		inline half4 LightingToonRamp (SurfaceOutput s, half3 lightDir, half atten)
		{
			#ifndef USING_DIRECTIONAL_LIGHT
			lightDir = normalize(lightDir);
			#endif

			half d = dot (s.Normal, lightDir)*0.5 + 0.5;
			half3 ramp = tex2D (_Ramp, float2(d,d)).rgb;

			half4 c;
			c.rgb = s.Albedo * _LightColor0.rgb * ramp * (atten * 2);
			c.a = 0;
			return c;
		}


		float4 _Color;
		float _Offset;
		float4 _HiColor;
		float _HighlightOffset;
		float _Brightness;
		float4 _SpecuColor;
		float4 _RimColor;
		float _RimPower;

		struct Input {
			float2 uv_MainTex : TEXCOORD0;
			float3 viewDir;
			float3 lightDir;
		};

		 void vert(inout appdata_full v, out Input o)
		    {
		        UNITY_INITIALIZE_OUTPUT(Input, o);
		        o.lightDir = WorldSpaceLightDir(v.vertex); // get the worldspace lighting direction
		    }


		void surf (Input IN, inout SurfaceOutput o) {
			float spec = dot(normalize(IN.viewDir + IN.lightDir), o.Normal); // specular based on view and light direction
			float cutOff = step(spec, _Offset); // cutoff for where base color is
			float3 baseAlbedo = _Color * cutOff; // base color
			float3 specularAlbedo = _SpecuColor * (1-cutOff) * _Brightness; // inverted base cutoff times specular color
			float highlight = saturate(dot(IN.lightDir, o.Normal)); // highlight based on light direction
			float4 highlightAlbedo =  (step(_HighlightOffset,highlight) * _HiColor); //glowing highlight
			o.Albedo = baseAlbedo + specularAlbedo  + highlightAlbedo; // result
			half rim = 1- saturate(dot (normalize(IN.viewDir), o.Normal)); // standard rim calculation   
			o.Emission += _RimColor.rgb * pow(rim, _RimPower); // rim lighting added to glowing highlight


		}
		ENDCG

	} 

	Fallback "Diffuse"
}