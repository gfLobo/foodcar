/** @type {import('next').NextConfig} */
const nextConfig = {
  reactStrictMode: true,
  async headers() {
    return [
      {
        source: '/car', // Substitua '/car' pela rota que está gerando o erro de conteúdo misto
        headers: [
          {
            key: 'Content-Security-Policy',
            value: "upgrade-insecure-requests;",
          },
        ],
      },
    ];
  },
}

module.exports = nextConfig;
