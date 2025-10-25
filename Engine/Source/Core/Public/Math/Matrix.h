#pragma once
struct FVector;
struct FVector4;
struct FQuaternion;

struct FMatrix
{
	/**
	* @brief 4x4 float 타입의 행렬
	*/
	#if defined(_MSC_VER) // MSVC 컴파일러
		__declspec(align(16))
	#else // GCC, Clang 등
		alignas(16)
	#endif
	union
	{
		/**
		 * @brief 스칼라 접근용
		 */
		float Data[4][4];
		/**
		 * @brief SIMD 접근용: 4개의 __m128 벡터로 접근
		 */
		__m128 V[4];
	};

	static FMatrix DxToUE;
	static FMatrix UEToDx;

	/**
	* @brief float 타입의 배열을 사용한 FMatrix의 기본 생성자
	*/
	FMatrix();

	/**
	* @brief float 타입의 param을 사용한 FMatrix의 기본 생성자
	*/
	FMatrix(
		float M00, float M01, float M02, float M03,
		float M10, float M11, float M12, float M13,
		float M20, float M21, float M22, float M23,
		float M30, float M31, float M32, float M33);

	FMatrix(const FVector&, const FVector&, const FVector&);

	FMatrix(const FVector4&, const FVector4&, const FVector4&);

	/**
	* @brief 항등행렬
	*/
	static FMatrix Identity();


	/**
	* @brief 두 행렬곱을 진행한 행렬을 반환하는 연산자 함수
	*/
	FMatrix operator*(const FMatrix& InOtherMatrix) const;
	void operator*=(const FMatrix& InOtherMatrix);

	/**
	* @brief Data의 행을 FVector4의 형태로 반환
	*/
	FVector4 operator[](uint32 i) const;

	/**
	* @brief Position의 정보를 행렬로 변환하여 제공하는 함수
	*/
	static FMatrix TranslationMatrix(const FVector& InOtherVector);
	static FMatrix TranslationMatrixInverse(const FVector& InOtherVector);

	/**
	* @brief Scale의 정보를 행렬로 변환하여 제공하는 함수
	*/
	static FMatrix ScaleMatrix(const FVector& InOtherVector);
	static FMatrix ScaleMatrixInverse(const FVector& InOtherVector);

	/**
	* @brief Rotation의 정보를 행렬로 변환하여 제공하는 함수
	*/
	static FMatrix RotationMatrix(const FVector& InOtherVector);

	static FMatrix CreateFromYawPitchRoll(const float yaw, const float pitch, const float roll);

	static FMatrix RotationMatrixInverse(const FVector& InOtherVector);

	/**
	* @brief X의 회전 정보를 행렬로 변환
	*/
	static FMatrix RotationX(float Radian);

	/**
	* @brief Y의 회전 정보를 행렬로 변환
	*/
	static FMatrix RotationY(float Radian);

	/**
	* @brief Y의 회전 정보를 행렬로 변환
	*/
	static FMatrix RotationZ(float Radian);

    static FMatrix GetModelMatrix(const FVector& Location, const FVector& Rotation, const FVector& Scale);

    static FMatrix GetModelMatrix(const FVector& Location, const FQuaternion& Rotation, const FVector& Scale);
    static FMatrix GetModelMatrixInverse(const FVector& Location, const FVector& Rotation, const FVector& Scale);

	static FMatrix GetModelMatrixInverse(const FVector& Location, const FQuaternion& Rotation, const FVector& Scale);
	static FVector4 VectorMultiply(const FVector4&, const FMatrix&);

	static FVector VectorMultiply(const FVector& v, const FMatrix& m);

	/**
	 * 이미 계산된 축(Axes) 벡터와 위치를 기반으로 뷰 행렬(View Matrix)을 생성
	 *
	 * @param Position  카메라(라이트)의 월드 위치
	 * @param Right     카메라의 X축 (월드 기준)
	 * @param Up        카메라의 Y축 (월드 기준)
	 * @param Forward   카메라의 Z축 (월드 기준)
	 * @return 계산된 뷰 행렬 (FMatrix)
	 */
	static FMatrix CreateViewFromAxes(const FVector& Position, const FVector& Right, const FVector& Up, const FVector& Forward);
	/**
	 * @brief (LH) 원근 투영 행렬을 생성합니다. (Z: 0-1 범위)
	 * @param FovYRadians 세로 화각 (라디안 단위)
	 * @param AspectRatio 종횡비 (Width / Height)
	 * @param NearZ 가까운 클립 평면
	 * @param FarZ 먼 클립 평면
	 */
	static FMatrix CreatePerspectiveFOV(float FovYRadians, float AspectRatio, float NearZ, float FarZ);
	/**
	 * 지정된 경계를 가지는 직교 투영(Orthographic) 행렬을 생성합니다.
	 * (DirectX, 왼손 좌표계, Z: 0~1 기준)
	 *
	 * @param Left   뷰 볼륨의 왼쪽 경계
	 * @param Right  뷰 볼륨의 오른쪽 경계
	 * @param Bottom 뷰 볼륨의 아래쪽 경계
	 * @param Top    뷰 볼륨의 위쪽 경계
	 * @param Near   뷰 볼륨의 가까운 경계
	 * @param Far    뷰 볼륨의 먼 경계
	 * @return 직교 투영 행렬
	 */
	static FMatrix CreateOrthographicOffCenter(float Left, float Right, float Bottom, float Top, float Near, float Far);

	FMatrix Transpose() const;

	FVector GetLocation() const;
	FVector GetRotation() const;
	FVector GetScale() const;
	FVector TransformPosition(const FVector& V) const;
};
