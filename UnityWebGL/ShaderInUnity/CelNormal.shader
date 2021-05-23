Shader "Custom/CelNormal"
{
	Properties
	{
		_MainTex ("Texture", 2D) = "white" {}
		_ShadowTint("Shadow Tint", Color) = (1,1,1,1)
		_OutlineWidth ("Outline Width", Range(0, 0.1)) = 0.01
	}

	SubShader
	{
		Tags { "RenderType"="Opaque" }
		LOD 100

		Pass
		{
			Tags
			{
				"LightMode" = "ForwardBase"
				"PassFlags" = "OnlyDirectional"
			}
			
			Cull Off

			Stencil 
			{
				Ref 1
				Comp Always
				Pass Replace
			}

			CGPROGRAM
			#pragma vertex vert
			#pragma fragment frag

			#include "UnityCG.cginc"

			struct appdata
			{
				float4 vertex : POSITION;
				float2 uv : TEXCOORD0;
				float3 normal : NORMAL;
				half4 color : COLOR;
			};

			struct v2f
			{
				float2 uv : TEXCOORD0;
				float4 vertex : SV_POSITION;
				float3 worldNormal : NORMAL;
				half4 color : COLOR;
				float3 viewDir : TEXCOORD1;
			};

			sampler2D _MainTex;
			float4 _MainTex_ST;
			half4 _ShadowTint;

			v2f vert (appdata v)
			{
				v2f o;
				o.vertex = UnityObjectToClipPos(v.vertex);
				o.uv = TRANSFORM_TEX(v.uv, _MainTex);
				o.worldNormal = UnityObjectToWorldNormal(v.normal);
				o.color = v.color;
				o.viewDir = WorldSpaceViewDir(v.vertex);
				return o;
			}

			static const half4 SPECULAR_COLOR = half4(1, 1, 1, 1);

			fixed4 frag (v2f i, half facing : VFACE) : SV_Target
			{
				float3 normal = normalize(i.worldNormal);
				half sign = facing > 0.5 ? 1.0 : -1.0;
				normal *= sign;
				float NdotL = dot(_WorldSpaceLightPos0, normal);
				float NdotL01 = NdotL * 0.5 + 0.5;
				half shadowMask = 1 - step(1 - i.color.r, NdotL01);
				float3 viewDir = normalize(i.viewDir);
				float3 halfVector = normalize(_WorldSpaceLightPos0 + viewDir);
				float NdotH = saturate(dot(halfVector, normal));
				float specularIntensity = i.color.g == 0 ? 0 : pow(NdotH, i.color.g * 500);
				half specularMask = step(0.5, specularIntensity);
				specularMask *= (1 - shadowMask);
				fixed4 texCol = tex2D(_MainTex, i.uv);
				half4 shadowCol = texCol * shadowMask * _ShadowTint;
				half4 col = lerp(texCol, shadowCol, shadowMask);
				col = lerp(col, SPECULAR_COLOR, specularMask);
				return col;
			}
			ENDCG
		}

		Pass
		{	
			Cull Off

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

		UsePass "Legacy Shaders/VertexLit/SHADOWCASTER"
	}
}