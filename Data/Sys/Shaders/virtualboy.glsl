void main()
{
	float4 c0 = Sample();
	float avg = (0.5 * c0.r) + (0.4 * c0.g) + (0.1 * c0.b);
	SetOutput(float4(avg, 0, 0, c0.a));
}
