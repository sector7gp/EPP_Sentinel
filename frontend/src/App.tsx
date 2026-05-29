import { Navigate, NavLink, Route, Routes, useNavigate } from "react-router-dom";
import LoginPage from "./pages/LoginPage";
import DashboardPage from "./pages/DashboardPage";
import ConfigPage from "./pages/ConfigPage";
import LogsPage from "./pages/LogsPage";

function PrivateRoute({ children }: { children: React.ReactNode }) {
  const token = localStorage.getItem("token");
  if (!token) return <Navigate to="/login" replace />;
  return <>{children}</>;
}

function Layout({ children }: { children: React.ReactNode }) {
  const navigate = useNavigate();
  return (
    <div className="app-shell">
      <nav className="nav">
        <span className="brand">EPP Sentinel</span>
        <NavLink to="/">Dashboard</NavLink>
        <NavLink to="/config">Configuración</NavLink>
        <NavLink to="/logs">Logs</NavLink>
        <button
          type="button"
          className="secondary"
          onClick={() => {
            localStorage.removeItem("token");
            navigate("/login");
          }}
        >
          Salir
        </button>
      </nav>
      <main className="main">{children}</main>
    </div>
  );
}

export default function App() {
  return (
    <Routes>
      <Route path="/login" element={<LoginPage />} />
      <Route
        path="/"
        element={
          <PrivateRoute>
            <Layout>
              <DashboardPage />
            </Layout>
          </PrivateRoute>
        }
      />
      <Route
        path="/config"
        element={
          <PrivateRoute>
            <Layout>
              <ConfigPage />
            </Layout>
          </PrivateRoute>
        }
      />
      <Route
        path="/logs"
        element={
          <PrivateRoute>
            <Layout>
              <LogsPage />
            </Layout>
          </PrivateRoute>
        }
      />
      <Route path="*" element={<Navigate to="/" replace />} />
    </Routes>
  );
}
